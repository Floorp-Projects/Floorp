/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all Servo Box<T> types used across bindings, for preprocessing */

// The first argument is the name of the Servo type used inside the Arc.
// This doesn't need to be accurate; it's only used to generate nice looking
// FFI function names.
//
// The second argument is the name of an opaque Gecko type that will
// correspond to the Servo type used inside the Box.  The convention for the
// the name of the opaque Gecko type is "RawServo{Type}", where {Type} is
// the name of the Servo type or something close to it.
//
// See the comment at the top of ServoBindingTypes.h for how to use these.
//
// If you add an entry to this file, you should also add an implementation of
// HasBoxFFI in Rust.
//
// TODO(emilio): We should remove the opaque type now, cbindgen should be able
// to just generate the forward declaration.

SERVO_BOXED_TYPE(StyleSet, RawServoStyleSet)
SERVO_BOXED_TYPE(AuthorStyles, RawServoAuthorStyles)
SERVO_BOXED_TYPE(SelectorList, RawServoSelectorList)
SERVO_BOXED_TYPE(SharedMemoryBuilder, RawServoSharedMemoryBuilder)
SERVO_BOXED_TYPE(SourceSizeList, RawServoSourceSizeList)
SERVO_BOXED_TYPE(UseCounters, StyleUseCounters)
