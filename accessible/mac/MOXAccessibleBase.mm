/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXAccessibleBase.h"

#import "MacSelectorMap.h"

#include "nsObjCExceptions.h"
#include "xpcAccessibleMacInterface.h"
#include "mozilla/Logging.h"
#include "gfxPlatform.h"

using namespace mozilla;
using namespace mozilla::a11y;

#undef LOG
mozilla::LogModule* GetMacAccessibilityLog() {
  static mozilla::LazyLogModule sLog("MacAccessibility");

  return sLog;
}
#define LOG(type, format, ...)                                             \
  do {                                                                     \
    if (MOZ_LOG_TEST(GetMacAccessibilityLog(), type)) {                    \
      NSString* msg = [NSString stringWithFormat:(format), ##__VA_ARGS__]; \
      MOZ_LOG(GetMacAccessibilityLog(), type, ("%s", [msg UTF8String]));   \
    }                                                                      \
  } while (0)

@interface NSObject (MOXAccessible)

// This NSObject conforms to MOXAccessible.
// This is needed to we know to mutate the value
// (get represented view, check isAccessibilityElement)
// before forwarding it to NSAccessibility.
- (BOOL)isMOXAccessible;

// Same as above, but this checks if the NSObject is an array with
// mozAccessible conforming objects.
- (BOOL)hasMOXAccessibles;

@end

@implementation NSObject (MOXAccessible)

- (BOOL)isMOXAccessible {
  return [self conformsToProtocol:@protocol(MOXAccessible)];
}

- (BOOL)hasMOXAccessibles {
  return [self isKindOfClass:[NSArray class]] &&
         [[(NSArray*)self firstObject] isMOXAccessible];
}

@end

// Private methods
@interface MOXAccessibleBase ()

- (BOOL)isSelectorSupported:(SEL)selector;

@end

@implementation MOXAccessibleBase

#pragma mark - mozAccessible/widget

- (BOOL)hasRepresentedView {
  return NO;
}

- (id)representedView {
  return nil;
}

- (BOOL)isRoot {
  return NO;
}

#pragma mark - mozAccessible/NSAccessibility

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ([self isExpired]) {
    return nil;
  }

  static NSMutableDictionary* attributesForEachClass = nil;

  if (!attributesForEachClass) {
    attributesForEachClass = [[NSMutableDictionary alloc] init];
  }

  NSMutableArray* attributes =
      attributesForEachClass [[self class]]
          ?: [[[NSMutableArray alloc] init] autorelease];

  NSDictionary* getters = mac::AttributeGetters();
  if (![attributes count]) {
    // Go through all our attribute getters, if they are supported by this class
    // advertise the attribute name.
    for (NSString* attribute in getters) {
      SEL selector = NSSelectorFromString(getters[attribute]);
      if ([self isSelectorSupported:selector]) {
        [attributes addObject:attribute];
      }
    }

    // If we have a delegate add all the text marker attributes.
    if ([self moxTextMarkerDelegate]) {
      [attributes addObjectsFromArray:[mac::TextAttributeGetters() allKeys]];
    }

    // We store a hash table with types as keys, and atttribute lists as values.
    // This lets us cache the atttribute list of each subclass so we only
    // need to gather its MOXAccessible methods once.
    // XXX: Uncomment when accessibilityAttributeNames is removed from all
    // subclasses. attributesForEachClass[[self class]] = attributes;
  }

  return attributes;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  if ([self isExpired]) {
    return nil;
  }

  id value = nil;
  NSDictionary* getters = mac::AttributeGetters();
  if (getters[attribute]) {
    SEL selector = NSSelectorFromString(getters[attribute]);
    if ([self isSelectorSupported:selector]) {
      value = [self performSelector:selector];
    }
  } else if (id textMarkerDelegate = [self moxTextMarkerDelegate]) {
    // If we have a delegate, check if attribute is a text marker
    // attribute and call the associated selector on the delegate
    // if so.
    NSDictionary* textMarkerGetters = mac::TextAttributeGetters();
    if (textMarkerGetters[attribute]) {
      SEL selector = NSSelectorFromString(textMarkerGetters[attribute]);
      if ([textMarkerDelegate respondsToSelector:selector]) {
        value = [textMarkerDelegate performSelector:selector];
      }
    }
  }

  if ([value isMOXAccessible]) {
    // If this is a MOXAccessible, get its represented view or filter it if
    // it should be ignored.
    value = [value isAccessibilityElement] ? GetObjectOrRepresentedView(value)
                                           : nil;
  }

  if ([value hasMOXAccessibles]) {
    // If this is an array of mozAccessibles, get each element's represented
    // view and remove it from the returned array if it should be ignored.
    NSUInteger arrSize = [value count];
    NSMutableArray* arr =
        [[[NSMutableArray alloc] initWithCapacity:arrSize] autorelease];
    for (NSUInteger i = 0; i < arrSize; i++) {
      id<mozAccessible> mozAcc = GetObjectOrRepresentedView(value[i]);
      if ([mozAcc isAccessibilityElement]) {
        [arr addObject:mozAcc];
      }
    }

    value = arr;
  }

  if (MOZ_LOG_TEST(GetMacAccessibilityLog(), LogLevel::Debug)) {
    if (MOZ_LOG_TEST(GetMacAccessibilityLog(), LogLevel::Verbose)) {
      LOG(LogLevel::Verbose, @"%@ attributeValue %@ => %@", self, attribute,
          value);
    } else if (![attribute isEqualToString:@"AXParent"] &&
               ![attribute isEqualToString:@"AXRole"] &&
               ![attribute isEqualToString:@"AXSubrole"] &&
               ![attribute isEqualToString:@"AXSize"] &&
               ![attribute isEqualToString:@"AXPosition"] &&
               ![attribute isEqualToString:@"AXRole"] &&
               ![attribute isEqualToString:@"AXChildren"]) {
      LOG(LogLevel::Debug, @"%@ attributeValue %@", self, attribute);
    }
  }

  return value;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ([self isExpired]) {
    return NO;
  }

  NSDictionary* setters = mac::AttributeSetters();
  if (setters[attribute]) {
    SEL selector = NSSelectorFromString(setters[attribute]);
    if ([self isSelectorSupported:selector]) {
      return YES;
    }
  } else if (id textMarkerDelegate = [self moxTextMarkerDelegate]) {
    // If we have a delegate, check text setters on delegate
    NSDictionary* textMarkerSetters = mac::TextAttributeSetters();
    if (textMarkerSetters[attribute]) {
      SEL selector = NSSelectorFromString(textMarkerSetters[attribute]);
      if ([textMarkerDelegate respondsToSelector:selector]) {
        return YES;
      }
    }
  }

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if ([self isExpired]) {
    return;
  }

  LOG(LogLevel::Debug, @"%@ setValueForattribute %@ = %@", self, attribute,
      value);

  NSDictionary* setters = mac::AttributeSetters();
  if (setters[attribute]) {
    SEL selector = NSSelectorFromString(setters[attribute]);
    if ([self isSelectorSupported:selector]) {
      [self performSelector:selector withObject:value];
    }
  } else if (id textMarkerDelegate = [self moxTextMarkerDelegate]) {
    // If we have a delegate, check if attribute is a text marker
    // attribute and call the associated selector on the delegate
    // if so.
    NSDictionary* textMarkerSetters = mac::TextAttributeSetters();
    if (textMarkerSetters[attribute]) {
      SEL selector = NSSelectorFromString(textMarkerSetters[attribute]);
      if ([textMarkerDelegate respondsToSelector:selector]) {
        [textMarkerDelegate performSelector:selector withObject:value];
      }
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (NSArray*)accessibilityActionNames {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ([self isExpired]) {
    return nil;
  }

  NSMutableArray* actionNames = [[[NSMutableArray alloc] init] autorelease];

  NSDictionary* actions = mac::Actions();
  for (NSString* action in actions) {
    SEL selector = NSSelectorFromString(actions[action]);
    if ([self isSelectorSupported:selector]) {
      [actionNames addObject:action];
    }
  }

  return actionNames;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)accessibilityPerformAction:(NSString*)action {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if ([self isExpired]) {
    return;
  }

  LOG(LogLevel::Debug, @"%@ performAction %@ ", self, action);

  NSDictionary* actions = mac::Actions();
  if (actions[action]) {
    SEL selector = NSSelectorFromString(actions[action]);
    if ([self isSelectorSupported:selector]) {
      [self performSelector:selector];
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  // by default we return whatever the MacOS API know about.
  // if you have custom actions, override.
  return NSAccessibilityActionDescription(action);
  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (NSArray*)accessibilityParameterizedAttributeNames {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ([self isExpired]) {
    return nil;
  }

  NSMutableArray* attributeNames = [[[NSMutableArray alloc] init] autorelease];

  NSDictionary* attributes = mac::ParameterizedAttributeGetters();
  for (NSString* attribute in attributes) {
    SEL selector = NSSelectorFromString(attributes[attribute]);
    if ([self isSelectorSupported:selector]) {
      [attributeNames addObject:attribute];
    }
  }

  // If we have a delegate add all the text marker attributes.
  if ([self moxTextMarkerDelegate]) {
    [attributeNames
        addObjectsFromArray:[mac::ParameterizedTextAttributeGetters() allKeys]];
  }

  return attributeNames;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ([self isExpired]) {
    return nil;
  }

  id value = nil;

  NSDictionary* getters = mac::ParameterizedAttributeGetters();
  if (getters[attribute]) {
    SEL selector = NSSelectorFromString(getters[attribute]);
    if ([self isSelectorSupported:selector]) {
      value = [self performSelector:selector withObject:parameter];
    }
  } else if (id textMarkerDelegate = [self moxTextMarkerDelegate]) {
    // If we have a delegate, check if attribute is a text marker
    // attribute and call the associated selector on the delegate
    // if so.
    NSDictionary* textMarkerGetters = mac::ParameterizedTextAttributeGetters();
    if (textMarkerGetters[attribute]) {
      SEL selector = NSSelectorFromString(textMarkerGetters[attribute]);
      if ([textMarkerDelegate respondsToSelector:selector]) {
        value = [textMarkerDelegate performSelector:selector
                                         withObject:parameter];
      }
    }
  }

  if (MOZ_LOG_TEST(GetMacAccessibilityLog(), LogLevel::Verbose)) {
    LOG(LogLevel::Verbose, @"%@ attributeValueForParam %@(%@) => %@", self,
        attribute, parameter, value);
  } else {
    LOG(LogLevel::Debug, @"%@ attributeValueForParam %@", self, attribute);
  }

  return value;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)accessibilityHitTest:(NSPoint)point {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  return GetObjectOrRepresentedView([self moxHitTest:point]);
  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)accessibilityFocusedUIElement {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  return GetObjectOrRepresentedView([self moxFocusedUIElement]);
  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (BOOL)isAccessibilityElement {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ([self isExpired]) {
    return YES;
  }

  id parent = [self moxParent];
  if (![parent isMOXAccessible]) {
    return YES;
  }

  return ![self moxIgnoreWithParent:parent];

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

#pragma mark - MOXAccessible protocol

- (NSNumber*)moxIndexForChildUIElement:(id)child {
  return @([[self moxUnignoredChildren] indexOfObject:child]);
}

- (id)moxTopLevelUIElement {
  return [self moxWindow];
}

- (id)moxHitTest:(NSPoint)point {
  return self;
}

- (id)moxFocusedUIElement {
  return self;
}

- (void)moxPostNotification:(NSString*)notification {
  [self moxPostNotification:notification withUserInfo:nil];
}

- (void)moxPostNotification:(NSString*)notification
               withUserInfo:(NSDictionary*)userInfo {
  if (MOZ_LOG_TEST(GetMacAccessibilityLog(), LogLevel::Verbose)) {
    LOG(LogLevel::Verbose, @"%@ notify %@ %@", self, notification, userInfo);
  } else {
    LOG(LogLevel::Debug, @"%@ notify %@", self, notification);
  }

  // This sends events via nsIObserverService to be consumed by our mochitests.
  xpcAccessibleMacEvent::FireEvent(self, notification, userInfo);

  if (gfxPlatform::IsHeadless()) {
    // Using a headless toolkit for tests and whatnot, posting accessibility
    // notification won't work.
    return;
  }

  if (![self isAccessibilityElement]) {
    // If this is an ignored object, don't expose it to system.
    return;
  }

  if (userInfo) {
    NSAccessibilityPostNotificationWithUserInfo(
        GetObjectOrRepresentedView(self), notification, userInfo);
  } else {
    NSAccessibilityPostNotification(GetObjectOrRepresentedView(self),
                                    notification);
  }
}

- (BOOL)moxBlockSelector:(SEL)selector {
  return NO;
}

- (NSArray*)moxChildren {
  return @[];
}

- (NSArray*)moxUnignoredChildren {
  NSMutableArray* unignoredChildren =
      [[[NSMutableArray alloc] init] autorelease];
  NSArray* allChildren = [self moxChildren];

  for (MOXAccessibleBase* nativeChild in allChildren) {
    if ([nativeChild moxIgnoreWithParent:self]) {
      // If this child should be ignored get its unignored children.
      // This will in turn recurse to any unignored descendants if the
      // child is ignored.
      [unignoredChildren
          addObjectsFromArray:[nativeChild moxUnignoredChildren]];
    } else {
      [unignoredChildren addObject:nativeChild];
    }
  }

  return unignoredChildren;
}

- (id<mozAccessible>)moxParent {
  return nil;
}

- (id<mozAccessible>)moxUnignoredParent {
  id nativeParent = [self moxParent];

  if (![nativeParent isAccessibilityElement]) {
    return [nativeParent moxUnignoredParent];
  }

  return GetObjectOrRepresentedView(nativeParent);
}

- (BOOL)moxIgnoreWithParent:(MOXAccessibleBase*)parent {
  return [parent moxIgnoreChild:self];
}

- (BOOL)moxIgnoreChild:(MOXAccessibleBase*)child {
  return NO;
}

- (id<MOXTextMarkerSupport>)moxTextMarkerDelegate {
  return nil;
}

- (BOOL)moxIsLiveRegion {
  return NO;
}

#pragma mark -

// objc-style description (from NSObject); not to be confused with the
// accessible description above.
- (NSString*)description {
  if (MOZ_LOG_TEST(GetMacAccessibilityLog(), LogLevel::Debug)) {
    if ([self isSelectorSupported:@selector(moxMozDebugDescription)]) {
      return [self moxMozDebugDescription];
    }
  }

  return [NSString stringWithFormat:@"<%@: %p %@>",
                                    NSStringFromClass([self class]), self,
                                    [self moxRole]];
}

- (BOOL)isExpired {
  return mIsExpired;
}

- (void)expire {
  MOZ_ASSERT(!mIsExpired, "expire called an expired mozAccessible!");

  mIsExpired = YES;

  [self moxPostNotification:NSAccessibilityUIElementDestroyedNotification];
}

- (id<MOXAccessible>)moxFindAncestor:(BOOL (^)(id<MOXAccessible> moxAcc,
                                               BOOL* stop))findBlock {
  for (id element = self; [element conformsToProtocol:@protocol(MOXAccessible)];
       element = [element moxUnignoredParent]) {
    BOOL stop = NO;
    if (findBlock(element, &stop)) {
      return element;
    }

    if (stop || ![element respondsToSelector:@selector(moxUnignoredParent)]) {
      break;
    }
  }

  return nil;
}

#pragma mark - Private

- (BOOL)isSelectorSupported:(SEL)selector {
  return
      [self respondsToSelector:selector] && ![self moxBlockSelector:selector];
}

@end
