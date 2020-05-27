/* -*- Mode: Objective-C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@protocol MOXAccessible

// The deepest descendant of the accessible subtree that contains the specified point.
// Forwarded from accessibilityHitTest.
- (id _Nullable)moxHitTest:(NSPoint)point;

// The deepest descendant of the accessible subtree that has the focus.
// Forwarded from accessibilityFocusedUIElement.
- (id _Nullable)moxFocusedUIElement;

// Sends a notification to any observing assistive applications.
- (void)moxPostNotification:(NSString* _Nonnull)notification;

@end
