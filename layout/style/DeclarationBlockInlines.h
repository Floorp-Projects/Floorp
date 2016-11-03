/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DeclarationBlockInlines_h
#define mozilla_DeclarationBlockInlines_h

#include "mozilla/css/Declaration.h"
#include "mozilla/ServoDeclarationBlock.h"

namespace mozilla {

MOZ_DEFINE_STYLO_METHODS(DeclarationBlock, css::Declaration, ServoDeclarationBlock)

MozExternalRefCountType
DeclarationBlock::AddRef()
{
  MOZ_STYLO_FORWARD(AddRef, ())
}

MozExternalRefCountType
DeclarationBlock::Release()
{
  MOZ_STYLO_FORWARD(Release, ())
}

already_AddRefed<DeclarationBlock>
DeclarationBlock::Clone() const
{
  RefPtr<DeclarationBlock> result;
  if (IsGecko()) {
    result = new css::Declaration(*AsGecko());
  } else {
    result = new ServoDeclarationBlock(*AsServo());
  }
  return result.forget();
}

already_AddRefed<DeclarationBlock>
DeclarationBlock::EnsureMutable()
{
#ifdef DEBUG
  if (IsGecko()) {
    AsGecko()->AssertNotExpanded();
  }
#endif
  if (!IsMutable()) {
    return Clone();
  }
  return do_AddRef(this);
}

void
DeclarationBlock::ToString(nsAString& aString) const
{
  MOZ_STYLO_FORWARD(ToString, (aString))
}

uint32_t
DeclarationBlock::Count() const
{
  MOZ_STYLO_FORWARD(Count, ())
}

bool
DeclarationBlock::GetNthProperty(uint32_t aIndex, nsAString& aReturn) const
{
  MOZ_STYLO_FORWARD(GetNthProperty, (aIndex, aReturn))
}

} // namespace mozilla

#endif // mozilla_DeclarationBlockInlines_h
