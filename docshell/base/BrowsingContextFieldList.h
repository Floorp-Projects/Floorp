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
MOZ_BC_FIELD(IsActive, bool)
MOZ_BC_FIELD(EmbedderPolicy, nsILoadInfo::CrossOriginEmbedderPolicy)
MOZ_BC_FIELD(OpenerPolicy, nsILoadInfo::CrossOriginOpenerPolicy)

// The current opener for this BrowsingContext. This is a weak reference, and
// stored as the opener ID.
MOZ_BC_FIELD(OpenerId, uint64_t)

MOZ_BC_FIELD(OnePermittedSandboxedNavigatorId, uint64_t)

// Window ID of the inner window which embeds this BrowsingContext.
MOZ_BC_FIELD(EmbedderInnerWindowId, uint64_t)

MOZ_BC_FIELD(HadOriginalOpener, bool)

MOZ_BC_FIELD(IsPopupSpam, bool)

// This field controls whether the browsing context is currently considered to
// be activated by a gesture.
MOZ_BC_FIELD(UserActivationState, UserActivation::State)

// Hold the audio muted state and should be used
// on top level browsing contexts only.
MOZ_BC_FIELD(Muted, bool)

MOZ_BC_FIELD(FeaturePolicy, RefPtr<mozilla::dom::FeaturePolicy>)

// See nsSandboxFlags.h for the possible flags.
MOZ_BC_FIELD(SandboxFlags, uint32_t)

// ScreenOrientation-related APIs
MOZ_BC_FIELD(CurrentOrientationAngle, float)

MOZ_BC_FIELD(CurrentOrientationType, mozilla::dom::OrientationType)

MOZ_BC_FIELD(HistoryID, nsID)

MOZ_BC_FIELD(InRDMPane, bool)

MOZ_BC_FIELD(Loading, bool)

// These field are used to store the states of autoplay media request on
// GeckoView only, and it would only be modified on the top level browsing
// context.
MOZ_BC_FIELD(GVAudibleAutoplayRequestStatus, GVAutoplayRequestStatus)
MOZ_BC_FIELD(GVInaudibleAutoplayRequestStatus, GVAutoplayRequestStatus)

MOZ_BC_FIELD(AncestorLoading, bool)

#undef MOZ_BC_FIELD
