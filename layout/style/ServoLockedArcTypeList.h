/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all Servo Arc<Locked<T>> types used across bindings */

// The argument is the name of the Servo type used inside the Arc.
//
// If you add an entry to this file, you should also add an
// impl_locked_arc_ffi!() call to servo/components/style/gecko/arc_types.rs, and
// maybe also a corresponding mapping between the types in
// layout/style/ServoBindings.toml.

SERVO_LOCKED_ARC_TYPE(CssRules)
SERVO_LOCKED_ARC_TYPE(DeclarationBlock)
SERVO_LOCKED_ARC_TYPE(StyleRule)
SERVO_LOCKED_ARC_TYPE(ImportRule)
SERVO_LOCKED_ARC_TYPE(Keyframe)
SERVO_LOCKED_ARC_TYPE(KeyframesRule)
SERVO_LOCKED_ARC_TYPE(MediaList)
SERVO_LOCKED_ARC_TYPE(PageRule)
SERVO_LOCKED_ARC_TYPE(FontFaceRule)
SERVO_LOCKED_ARC_TYPE(CounterStyleRule)
