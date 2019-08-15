/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Fields are, by default, settable by any process and readable by any process.
// Racy sets will be resolved as-if they occurred in the order the parent
// process finds out about them.
//
// Process restrictions may be added by declaring a method `MaySet{name}` on
// `BrowsingContext`.
MOZ_BC_FIELD(Name, nsString)
MOZ_BC_FIELD(Closed, bool)
MOZ_BC_FIELD(EmbedderPolicy, nsILoadInfo::CrossOriginEmbedderPolicy)
MOZ_BC_FIELD(OpenerPolicy, nsILoadInfo::CrossOriginOpenerPolicy)

// The current opener for this BrowsingContext. This is a weak reference, and
// stored as the opener ID.
MOZ_BC_FIELD(OpenerId, uint64_t)

MOZ_BC_FIELD(OnePermittedSandboxedNavigatorId, uint64_t)

// Toplevel browsing contexts only. This field controls whether the browsing
// context is currently considered to be activated by a gesture.
MOZ_BC_FIELD(IsActivatedByUserGesture, bool)

#undef MOZ_BC_FIELD
