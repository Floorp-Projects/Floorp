/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_NotifyUpdatedDictionaries_h
#define mozilla_dom_ContentParent_NotifyUpdatedDictionaries_h

// Avoid including ContentParent.h because it ends up including WebGLTypes.h,
// where we have our mozilla::malloc(ForbidNarrowing<size_t>) overrides, which
// cause issues with the way hunspell overrides malloc for itself.
namespace mozilla::dom {
void ContentParent_NotifyUpdatedDictionaries();
}  // namespace mozilla::dom

#endif  // mozilla_dom_ContentParent_NotifyUpdatedDictionaries_h
