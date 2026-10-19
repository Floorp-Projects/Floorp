// SPDX-License-Identifier: MPL-2.0
// External observation only. Never requests Screen Recording or AX access.
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <ImageIO/ImageIO.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <unistd.h>

static NSString *boundedString(id value) {
  if (![value isKindOfClass:NSString.class])
    return @"";
  return [value substringToIndex:MIN([value length], 256)];
}

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    if (argc != 4)
      return 64;
    NSString *output = [NSString stringWithUTF8String:argv[1]];
    NSString *label = [NSString stringWithUTF8String:argv[2]];
    NSArray *owned = [[NSString stringWithUTF8String:argv[3]] componentsSeparatedByString:@","];
    BOOL preflight = CGPreflightScreenCaptureAccess();
    NSMutableArray *windows = [NSMutableArray array];
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    for (NSDictionary *item in (__bridge NSArray *)list) {
      if (windows.count == 256)
        break;
      NSNumber *pid = item[(__bridge NSString *)kCGWindowOwnerPID];
      [windows addObject:@{
        @"pid" : pid ?: @0,
        @"ownedProcess" : @([owned containsObject:pid.stringValue]),
        @"owner" : boundedString(item[(__bridge NSString *)kCGWindowOwnerName]),
        @"name" : boundedString(item[(__bridge NSString *)kCGWindowName]),
        @"window" : item[(__bridge NSString *)kCGWindowNumber] ?: @0,
        @"layer" : item[(__bridge NSString *)kCGWindowLayer] ?: @0,
        @"bounds" : item[(__bridge NSString *)kCGWindowBounds] ?: @{},
        @"alpha" : item[(__bridge NSString *)kCGWindowAlpha] ?: @0
      }];
    }
    if (list)
      CFRelease(list);
    NSMutableDictionary *record = [@{
      @"label" : label,
      @"observedAt" : @([[NSDate date] timeIntervalSince1970]),
      @"observerPid" : @(getpid()),
      @"preflightScreenCaptureAccess" : @(preflight),
      @"accessibilityTrusted" : @(AXIsProcessTrusted()),
      @"permissionScope" : @"external observer process only; no browser permission inference",
      @"windows" : windows,
      @"screenshotRequested" : @NO,
      @"screenshotWritten" : @NO
    } mutableCopy];
    // An actual screenshot is attempted only with existing observer access.
    // Both APIs below are public and do not present a sharing picker.
    if (preflight) {
      if (@available(macOS 14.0, *)) {
        record[@"screenshotRequested"] = @YES;
        __block BOOL done = NO;
        [SCShareableContent
            getShareableContentExcludingDesktopWindows:NO
                                   onScreenWindowsOnly:YES
                                     completionHandler:^(SCShareableContent *content, NSError *error) {
                                       dispatch_async(dispatch_get_main_queue(), ^{
                                         if (error) {
                                           record[@"screenshotError"] = error.localizedDescription;
                                           done = YES;
                                           return;
                                         }
                                         SCDisplay *display = nil;
                                         for (SCDisplay *candidate in content.displays) {
                                           if (candidate.displayID == CGMainDisplayID())
                                             display = candidate;
                                         }
                                         if (!display) {
                                           record[@"screenshotError"] = @"Main display not returned";
                                           done = YES;
                                           return;
                                         }
                                         SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display
                                                                                           excludingWindows:@[]];
                                         SCStreamConfiguration *configuration = [SCStreamConfiguration new];
                                         double scale = MIN(
                                             1.0, MIN(1920.0 / MAX(1, display.width), 1080.0 / MAX(1, display.height)));
                                         configuration.width = MAX(1, (size_t)(display.width * scale));
                                         configuration.height = MAX(1, (size_t)(display.height * scale));
                                         configuration.showsCursor = YES;
                                         record[@"screenshotGeometry"] = @{
                                           @"displayId" : @(display.displayID),
                                           @"displayWidth" : @(display.width),
                                           @"displayHeight" : @(display.height),
                                           @"imageWidth" : @(configuration.width),
                                           @"imageHeight" : @(configuration.height)
                                         };
                                         [SCScreenshotManager
                                             captureImageWithFilter:filter
                                                      configuration:configuration
                                                  completionHandler:^(CGImageRef image, NSError *captureError) {
                                                    if (image)
                                                      CGImageRetain(image);
                                                    dispatch_async(dispatch_get_main_queue(), ^{
                                                      if (captureError || !image) {
                                                        record[@"screenshotErro"
                                                               @"r"] = captureError.localizedDescription
                                                                           ?: @"No image "
                                                                              @"returned";
                                                      } else {
                                                        NSURL *url = [NSURL fileURLWithPath:output];
                                                        CGImageDestinationRef destination =
                                                            CGImageDestinationCreateWithURL(
                                                                (__bridge CFURLRef)url, CFSTR("public.png"), 1, NULL);
                                                        if (destination) {
                                                          CGImageDestinationAddImage(destination, image, NULL);
                                                          record[@"screenshotWrit"
                                                                 @"ten"] = @(CGImageDestinationFinalize(destination));
                                                          CFRelease(destination);
                                                        } else {
                                                          record[@"screenshotError"] = @"PNG destination "
                                                                                       @"unavailable";
                                                        }
                                                      }
                                                      if (image)
                                                        CGImageRelease(image);
                                                      done = YES;
                                                    });
                                                  }];
                                       });
                                     }];
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:2.5];
        while (!done && deadline.timeIntervalSinceNow > 0) {
          [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                   beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
        }
        if (!done)
          record[@"screenshotError"] = @"Public screenshot API deadline";
      } else {
        record[@"screenshotSkipped"] = @"SCScreenshotManager unavailable";
      }
    } else {
      record[@"screenshotSkipped"] = @"Observer has no existing Screen Recording access";
    }
    NSData *data = [NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted error:nil];
    if (!data)
      return 65;
    fwrite(data.bytes, 1, data.length, stdout);
    fputc('\n', stdout);
    return 0;
  }
}
