// SPDX-License-Identifier: MPL-2.0
// Inspect only unless explicitly invoked with the one reviewed action flag.
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

static NSMutableArray *AXReadErrors;

static id Read(AXUIElementRef element, CFStringRef attribute) {
  CFTypeRef value = NULL;
  AXError error = AXUIElementCopyAttributeValue(element, attribute, &value);
  if (error == kAXErrorSuccess) return CFBridgingRelease(value);
  if (value) CFRelease(value);
  // Unsupported/no-value attributes are normal leaves. Other failures must
  // never turn an incomplete AX graph into a unique, actionable button.
  if (error != kAXErrorAttributeUnsupported && error != kAXErrorNoValue) {
    [AXReadErrors addObject:@{ @"attribute": (__bridge NSString *)attribute,
                              @"error": @(error) }];
  }
  return nil;
}

static NSString *Text(id value) {
  return [value isKindOfClass:NSString.class] ? [value substringToIndex:MIN([value length], 256)] : @"";
}

static BOOL Frame(AXUIElementRef element, CGRect *frame) {
  id position = Read(element, kAXPositionAttribute);
  id size = Read(element, kAXSizeAttribute);
  CGPoint point;
  CGSize dimensions;
  if (!position || !size || CFGetTypeID((__bridge CFTypeRef)position) != AXValueGetTypeID() ||
      CFGetTypeID((__bridge CFTypeRef)size) != AXValueGetTypeID() ||
      !AXValueGetValue((__bridge AXValueRef)position, kAXValueCGPointType, &point) ||
      !AXValueGetValue((__bridge AXValueRef)size, kAXValueCGSizeType, &dimensions))
    return NO;
  *frame = (CGRect){point, dimensions};
  return isfinite(point.x) && isfinite(point.y) && isfinite(dimensions.width) && isfinite(dimensions.height) &&
         dimensions.width > 0 && dimensions.height > 0;
}

static NSDictionary *VisibleControls(void) {
  CFArrayRef raw = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
  NSDictionary *match = nil;
  NSUInteger count = 0;
  for (NSDictionary *window in (__bridge NSArray *)raw) {
    if ([window[(__bridge NSString *)kCGWindowOwnerName] isEqual:@"Control Center"] &&
        [window[(__bridge NSString *)kCGWindowName] isEqual:@"SharedContentPickerDisplayControls"] &&
        [window[(__bridge NSString *)kCGWindowAlpha] doubleValue] > 0.0) {
      match = window;
      ++count;
    }
  }
  if (raw)
    CFRelease(raw);
  return count == 1 ? match : nil;
}

static BOOL OwnedGreenSourceVisible(NSString *binary) {
  CFArrayRef raw = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
  BOOL found = NO;
  for (NSDictionary *window in (__bridge NSArray *)raw) {
    pid_t pid = [window[(__bridge NSString *)kCGWindowOwnerPID] intValue];
    if (pid > 0 && getpgid(pid) == getpgrp() &&
        [window[(__bridge NSString *)kCGWindowName] isEqual:@"floorp-desktop-screen-green-source"] &&
        [[NSRunningApplication runningApplicationWithProcessIdentifier:pid].executableURL.path isEqual:binary])
      found = YES;
  }
  if (raw) CFRelease(raw);
  return found;
}

// Bind the accessible display-specific label to the actual public main display.
// NSScreen names have not been observed in the preceding run; retain the real
// values here and fail closed unless identity and geometry agree at action time.
static NSDictionary *MainDisplayBinding(void) {
  CGDirectDisplayID displayID = CGMainDisplayID();
  CGRect cgFrame = CGDisplayBounds(displayID);
  NSScreen *matched = nil;
  NSUInteger count = 0;
  for (NSScreen *screen in NSScreen.screens) {
    NSNumber *number = screen.deviceDescription[@"NSScreenNumber"];
    if ([number isKindOfClass:NSNumber.class] && number.unsignedIntValue == displayID) {
      matched = screen;
      ++count;
    }
  }
  NSString *name = matched.localizedName;
  CGRect nsFrame = matched ? NSRectToCGRect(matched.frame) : CGRectZero;
  BOOL identity = count == 1 && [name isKindOfClass:NSString.class] && name.length > 0 &&
                  name.length <= 128 && CGRectEqualToRect(nsFrame, cgFrame);
  return @{
    @"cgMainDisplayID" : @(displayID),
    @"matchedNSScreenCount" : @(count),
    @"nsScreenNumber" : matched.deviceDescription[@"NSScreenNumber"] ?: @0,
    @"localizedName" : name ?: @"",
    @"cgDisplayFrame" : CFBridgingRelease(CGRectCreateDictionaryRepresentation(cgFrame)),
    @"nsScreenFrame" : CFBridgingRelease(CGRectCreateDictionaryRepresentation(nsFrame)),
    @"identityMatches" : @(identity),
    @"exactDisplayAXLabel" : identity ? [@"Share " stringByAppendingString:name] : @""
  };
}

static BOOL Matches(AXUIElementRef element, pid_t expectedPid, CGRect controls,
                    NSString *displayLabel, CGRect *frame) {
  pid_t actualPid = 0;
  id enabled = Read(element, kAXEnabledAttribute);
  NSString *title = Text(Read(element, kAXTitleAttribute));
  NSString *description = Text(Read(element, kAXDescriptionAttribute));
  CFArrayRef rawActions = NULL;
  BOOL press = AXUIElementCopyActionNames(element, &rawActions) == kAXErrorSuccess &&
               [(__bridge NSArray *)rawActions containsObject:(__bridge NSString *)kAXPressAction];
  if (rawActions)
    CFRelease(rawActions);
  return AXUIElementGetPid(element, &actualPid) == kAXErrorSuccess && actualPid == expectedPid &&
         [Text(Read(element, kAXRoleAttribute)) isEqual:(__bridge NSString *)kAXButtonRole] &&
         ([title isEqual:@"Share Entire Screen"] || [description isEqual:@"Share Entire Screen"] ||
          [title isEqual:displayLabel] || [description isEqual:displayLabel]) && enabled &&
         CFGetTypeID((__bridge CFTypeRef)enabled) == CFBooleanGetTypeID() && [enabled boolValue] && press &&
         Frame(element, frame) && CGRectContainsRect(controls, *frame);
}

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    if (argc != 3 || (strcmp(argv[1], "--inspect-only") && strcmp(argv[1], "--press-share-entire-screen")))
      return 64;
    BOOL requested = !strcmp(argv[1], "--press-share-entire-screen");
    AXReadErrors = [NSMutableArray array];
    NSString *expectedBinary = [NSString stringWithUTF8String:argv[2]];
    NSMutableDictionary *record = [@{
      @"observerPid" : @(getpid()),
      @"observedAt" : @([[NSDate date] timeIntervalSince1970]),
      @"accessibilityTrusted" : @(AXIsProcessTrusted()),
      @"preflightScreenCaptureAccess" : @(CGPreflightScreenCaptureAccess()),
      @"permissionScope" : @"external observer only; never requests AX or Screen Recording access",
      @"pressRequested" : @(requested),
      @"pressAttempted" : @NO,
      @"pressSucceeded" : @NO,
      @"networkOrSettingsPromptInteraction" : @NO
    } mutableCopy];
    NSDictionary *displayBinding = MainDisplayBinding();
    NSString *displayLabel = displayBinding[@"exactDisplayAXLabel"];
    record[@"mainDisplayBinding"] = displayBinding;
    NSDictionary *controls = VisibleControls();
    CGRect controlsFrame;
    pid_t pid = [controls[(__bridge NSString *)kCGWindowOwnerPID] intValue];
    NSRunningApplication *application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    BOOL frameOK =
        controls && CGRectMakeWithDictionaryRepresentation(
                        (__bridge CFDictionaryRef)controls[(__bridge NSString *)kCGWindowBounds], &controlsFrame);
    CGRect display = CGDisplayBounds(CGMainDisplayID());
    BOOL identityOK = [displayBinding[@"identityMatches"] boolValue] && frameOK && pid > 0 &&
                      [application.bundleIdentifier isEqual:@"com.apple.controlcenter"] &&
                      [application.bundleURL.path isEqual:@"/System/Library/CoreServices/ControlCenter.app"] &&
                      controlsFrame.origin.x == display.origin.x && controlsFrame.origin.y == display.origin.y &&
                      controlsFrame.size.width == display.size.width && controlsFrame.size.height == 60;
    record[@"controlsWindow"] = controls ?: @{};
    record[@"controlCenterBundle"] = application.bundleIdentifier ?: @"";
    record[@"controlCenterBundlePath"] = application.bundleURL.path ?: @"";
    record[@"ownerIdentityMatches"] = @(identityOK);
    record[@"expectedBrowserExecutable"] = expectedBinary;
    record[@"ownedGreenSourceVisible"] = @(OwnedGreenSourceVisible(expectedBinary));
    NSMutableArray *tree = [NSMutableArray array];
    NSMutableArray *candidates = [NSMutableArray array];
    BOOL complete = NO;
    BOOL truncated = NO;
    if (AXIsProcessTrusted() && CGPreflightScreenCaptureAccess() && identityOK && OwnedGreenSourceVisible(expectedBinary)) {
      AXUIElementRef app = AXUIElementCreateApplication(pid);
      AXUIElementSetMessagingTimeout(app, 0.1);
      NSMutableArray *queue = [NSMutableArray arrayWithObject:@{@"element" : (__bridge id)app, @"depth" : @0}];
      NSMutableArray *seen = [NSMutableArray array];
      CFAbsoluteTime end = CFAbsoluteTimeGetCurrent() + 2.0;
      while (queue.count && tree.count < 256 && CFAbsoluteTimeGetCurrent() < end) {
        NSDictionary *item = queue.firstObject;
        [queue removeObjectAtIndex:0];
        id object = item[@"element"];
        BOOL duplicate = NO;
        for (id prior in seen)
          if (CFEqual((__bridge CFTypeRef)prior, (__bridge CFTypeRef)object))
            duplicate = YES;
        if (duplicate)
          continue;
        [seen addObject:object];
        AXUIElementRef element = (__bridge AXUIElementRef)object;
        NSUInteger depth = [item[@"depth"] unsignedIntegerValue];
        CGRect frame;
        BOOL hasFrame = Frame(element, &frame);
        NSMutableDictionary *node = [@{
          @"index" : @(tree.count),
          @"depth" : @(depth),
          @"role" : Text(Read(element, kAXRoleAttribute)),
          @"title" : Text(Read(element, kAXTitleAttribute)),
          @"description" : Text(Read(element, kAXDescriptionAttribute))
        } mutableCopy];
        if (hasFrame)
          node[@"frame"] = CFBridgingRelease(CGRectCreateDictionaryRepresentation(frame));
        if (Matches(element, pid, controlsFrame, displayLabel, &frame)) {
          node[@"exactShareEntireScreenCandidate"] = @YES;
          [candidates addObject:object];
        }
        [tree addObject:node];
        {
          for (NSString *attribute in
              @[ (__bridge NSString *)kAXWindowsAttribute, (__bridge NSString *)kAXChildrenAttribute ]) {
            id children = Read(element, (__bridge CFStringRef)attribute);
            if ([children isKindOfClass:NSArray.class]) {
              if (depth >= 10 && [children count]) {
                truncated = YES;
                continue;
              }
              for (id child in children) {
                if (queue.count >= 512) {
                  truncated = YES;
                  break;
                }
                if (CFGetTypeID((__bridge CFTypeRef)child) == AXUIElementGetTypeID())
                  [queue addObject:@{@"element" : child, @"depth" : @(depth + 1)}];
              }
            }
          }
        }
      }
      complete = !queue.count && !truncated && !AXReadErrors.count;
      CFRelease(app);
    }
    record[@"tree"] = tree;
    record[@"treeCompleteWithinBound"] = @(complete);
    record[@"exactCandidateCount"] = @(candidates.count);
    if (requested && complete && candidates.count == 1) {
      NSDictionary *fresh = VisibleControls();
      NSDictionary *freshDisplayBinding = MainDisplayBinding();
      record[@"immediateMainDisplayBinding"] = freshDisplayBinding;
      CGRect freshFrame, buttonFrame;
      BOOL same =
          [freshDisplayBinding isEqual:displayBinding] && fresh &&
          [fresh[(__bridge NSString *)kCGWindowNumber] isEqual:controls[(__bridge NSString *)kCGWindowNumber]] &&
          [fresh[(__bridge NSString *)kCGWindowOwnerPID] intValue] == pid &&
          CGRectMakeWithDictionaryRepresentation(
              (__bridge CFDictionaryRef)fresh[(__bridge NSString *)kCGWindowBounds], &freshFrame) &&
          CGRectEqualToRect(freshFrame, controlsFrame) &&
          Matches((__bridge AXUIElementRef)candidates[0], pid, freshFrame, displayLabel, &buttonFrame);
      record[@"immediateRevalidationMatches"] = @(same);
      if (same && !AXReadErrors.count && AXIsProcessTrusted() && CGPreflightScreenCaptureAccess() && OwnedGreenSourceVisible(expectedBinary)) {
        record[@"pressAttempted"] = @YES;
        AXError result = AXUIElementPerformAction((__bridge AXUIElementRef)candidates[0], kAXPressAction);
        record[@"pressAXError"] = @(result);
        record[@"pressSucceeded"] = @(result == kAXErrorSuccess);
      }
    }
    record[@"axUnexpectedReadErrors"] = AXReadErrors;
    NSData *data = [NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted error:nil];
    if (!data)
      return 65;
    fwrite(data.bytes, 1, data.length, stdout);
    fputc('\n', stdout);
    return 0;
  }
}
