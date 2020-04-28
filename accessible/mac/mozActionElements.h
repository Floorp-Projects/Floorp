/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import "mozAccessible.h"

/* Simple subclasses for things like checkboxes, buttons, etc. */

@interface mozButtonAccessible : mozAccessible {
}
- (BOOL)hasPopup;
@end

@interface mozCheckboxAccessible : mozButtonAccessible
// returns one of the constants defined in CheckboxValue
- (int)isChecked;
@end

/**
 * Accessible for a PANE
 */
@interface mozPaneAccessible : mozAccessible

@end

/**
 * Base accessible for an incrementable
 */
@interface mozIncrementableAccessible : mozAccessible

@end
