/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXAccessibleBase.h"

#import "MacSelectorMap.h"

#include "nsObjCExceptions.h"
#include "xpcAccessibleMacInterface.h"

using namespace mozilla::a11y;

@interface NSObject (MOXAccessible)

// This NSObject conforms to mozAccessible.
// This is needed to we know to mutate the value
// (get represented view, check isAccessibilityElement)
// before forwarding it to NSAccessibility.
- (BOOL)isMozAccessible;

// Same as above, but this checks if the NSObject is an array with
// mozAccessible conforming objects.
- (BOOL)hasMozAccessibles;

@end

@implementation NSObject (MOXAccessible)

- (BOOL)isMozAccessible {
  return [self conformsToProtocol:@protocol(mozAccessible)];
}

- (BOOL)hasMozAccessibles {
  return [self isKindOfClass:[NSArray class]] && [[(NSArray*)self firstObject] isMozAccessible];
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isExpired]) {
    return nil;
  }

  static NSMutableDictionary* attributesForEachClass = nil;

  if (!attributesForEachClass) {
    attributesForEachClass = [[NSMutableDictionary alloc] init];
  }

  NSMutableArray* attributes =
      attributesForEachClass [[self class]] ?: [[NSMutableArray alloc] init];

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
    // XXX: Uncomment when accessibilityAttributeNames is removed from all subclasses.
    // attributesForEachClass[[self class]] = attributes;
  }

  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
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

  if ([value isMozAccessible]) {
    // If this is a mozAccessible, get its represented view and filter it if
    // it should be ignored.
    value = GetObjectOrRepresentedView(value);
    return [value isAccessibilityElement] ? value : nil;
  }

  if ([value hasMozAccessibles]) {
    // If this is an array of mozAccessibles, get each element's represented view
    // and remove it from the returned array if it should be ignored.
    NSUInteger arrSize = [value count];
    NSMutableArray* arr = [[NSMutableArray alloc] initWithCapacity:arrSize];
    for (NSUInteger i = 0; i < arrSize; i++) {
      id<mozAccessible> mozAcc = GetObjectOrRepresentedView(value[i]);
      if ([mozAcc isAccessibilityElement]) {
        [arr addObject:mozAcc];
      }
    }

    return arr;
  }

  return value;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([self isExpired]) {
    return NO;
  }

  NSDictionary* setters = mac::AttributeSetters();
  if (setters[attribute]) {
    SEL selector = NSSelectorFromString(setters[attribute]);
    return ([self isSelectorSupported:selector]);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([self isExpired]) {
    return;
  }

  NSDictionary* setters = mac::AttributeSetters();
  if (setters[attribute]) {
    SEL selector = NSSelectorFromString(setters[attribute]);
    if ([self isSelectorSupported:selector]) {
      [self performSelector:selector withObject:value];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSArray*)accessibilityActionNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isExpired]) {
    return nil;
  }

  NSMutableArray* actionNames = [[NSMutableArray alloc] init];

  NSDictionary* actions = mac::Actions();
  for (NSString* action in actions) {
    SEL selector = NSSelectorFromString(actions[action]);
    if ([self isSelectorSupported:selector]) {
      [actionNames addObject:action];
    }
  }

  return actionNames;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString*)action {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([self isExpired]) {
    return;
  }

  NSDictionary* actions = mac::Actions();
  if (actions[action]) {
    SEL selector = NSSelectorFromString(actions[action]);
    if ([self isSelectorSupported:selector]) {
      [self performSelector:selector];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  // by default we return whatever the MacOS API know about.
  // if you have custom actions, override.
  return NSAccessibilityActionDescription(action);
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)accessibilityParameterizedAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isExpired]) {
    return nil;
  }

  NSMutableArray* attributeNames = [[NSMutableArray alloc] init];

  NSDictionary* attributes = mac::ParameterizedAttributeGetters();
  for (NSString* attribute in attributes) {
    SEL selector = NSSelectorFromString(attributes[attribute]);
    if ([self isSelectorSupported:selector]) {
      [attributeNames addObject:attribute];
    }
  }

  // If we have a delegate add all the text marker attributes.
  if ([self moxTextMarkerDelegate]) {
    [attributeNames addObjectsFromArray:[mac::ParameterizedTextAttributeGetters() allKeys]];
  }

  return attributeNames;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isExpired]) {
    return nil;
  }

  NSDictionary* getters = mac::ParameterizedAttributeGetters();
  if (getters[attribute]) {
    SEL selector = NSSelectorFromString(getters[attribute]);
    if ([self isSelectorSupported:selector]) {
      return [self performSelector:selector withObject:parameter];
    }
  } else if (id textMarkerDelegate = [self moxTextMarkerDelegate]) {
    // If we have a delegate, check if attribute is a text marker
    // attribute and call the associated selector on the delegate
    // if so.
    NSDictionary* textMarkerGetters = mac::ParameterizedTextAttributeGetters();
    if (textMarkerGetters[attribute]) {
      SEL selector = NSSelectorFromString(textMarkerGetters[attribute]);
      if ([textMarkerDelegate respondsToSelector:selector]) {
        return [textMarkerDelegate performSelector:selector withObject:parameter];
      }
    }
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityHitTest:(NSPoint)point {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return [self moxHitTest:point];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityFocusedUIElement {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return [self moxFocusedUIElement];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isAccessibilityElement {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([self isExpired]) {
    return YES;
  }

  id parent = [self moxParent];

  return ![self moxIgnoreWithParent:parent];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

#pragma mark - MOXAccessible protocol

- (id)moxHitTest:(NSPoint)point {
  return GetObjectOrRepresentedView(self);
}

- (id)moxFocusedUIElement {
  return GetObjectOrRepresentedView(self);
}

- (void)moxPostNotification:(NSString*)notification {
  // This sends events via nsIObserverService to be consumed by our mochitests.
  xpcAccessibleMacInterface::FireEvent(self, notification);

  if (gfxPlatform::IsHeadless()) {
    // Using a headless toolkit for tests and whatnot, posting accessibility
    // notification won't work.
    return;
  }

  if (![self isAccessibilityElement]) {
    // If this is an ignored object, don't expose it to system.
    return;
  }

  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self), notification);
}

- (BOOL)moxBlockSelector:(SEL)selector {
  return NO;
}

- (id<MOXTextMarkerSupport>)moxTextMarkerDelegate {
  return nil;
}

- (NSArray*)moxChildren {
  return @[];
}

- (NSArray*)moxUnignoredChildren {
  NSMutableArray* unignoredChildren = [[NSMutableArray alloc] init];
  NSArray* allChildren = [self moxChildren];

  for (MOXAccessibleBase* nativeChild in allChildren) {
    if ([nativeChild moxIgnoreWithParent:self]) {
      // If this child should be ignored get its unignored children.
      // This will in turn recurse to any unignored descendants if the
      // child is ignored.
      [unignoredChildren addObjectsFromArray:[nativeChild moxUnignoredChildren]];
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

#pragma mark -

- (BOOL)isExpired {
  return mIsExpired;
}

- (void)expire {
  MOZ_ASSERT(!mIsExpired, "expire called an expired mozAccessible!");

  mIsExpired = YES;

  [self moxPostNotification:NSAccessibilityUIElementDestroyedNotification];
}

#pragma mark - Private

- (BOOL)isSelectorSupported:(SEL)selector {
  return [self respondsToSelector:selector] && ![self moxBlockSelector:selector];
}

@end
