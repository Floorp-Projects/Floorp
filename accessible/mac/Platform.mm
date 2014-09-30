/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "Platform.h"

#include "nsAppShell.h"

namespace mozilla {
namespace a11y {

// Mac a11y whitelisting
static bool sA11yShouldBeEnabled = false;

bool
ShouldA11yBeEnabled()
{
  EPlatformDisabledState disabledState = PlatformDisabledState();
  return (disabledState == ePlatformIsForceEnabled) || ((disabledState == ePlatformIsEnabled) && sA11yShouldBeEnabled);
}

void
PlatformInit()
{
}

void
PlatformShutdown()
{
}

void
ProxyCreated(ProxyAccessible*)
{
}

void
ProxyDestroyed(ProxyAccessible*)
{
}
}
}

@interface GeckoNSApplication(a11y)
-(void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;
@end

@implementation GeckoNSApplication(a11y)

-(void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute
{
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"])
    mozilla::a11y::sA11yShouldBeEnabled = ([value intValue] == 1);

  return [super accessibilitySetValue:value forAttribute:attribute];
}

@end

