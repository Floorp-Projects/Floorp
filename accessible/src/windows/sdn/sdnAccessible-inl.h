/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_sdnAccessible_inl_h_
#define mozilla_a11y_sdnAccessible_inl_h_

#include "sdnAccessible.h"

#include "DocAccessible.h"
#include "nsAccessibilityService.h"

namespace mozilla {
namespace a11y {

inline DocAccessible*
sdnAccessible::GetDocument() const
{
  return GetExistingDocAccessible(mNode->OwnerDoc());
}

inline Accessible*
sdnAccessible::GetAccessible() const
{
  DocAccessible* document = GetDocument();
  return document ? document->GetAccessible(mNode) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_sdnAccessible_inl_h_
