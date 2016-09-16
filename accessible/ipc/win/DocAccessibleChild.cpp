/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "Accessible-inl.h"
#include "mozilla/a11y/PlatformChild.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace a11y {

static StaticAutoPtr<PlatformChild> sPlatformChild;

DocAccessibleChild::DocAccessibleChild(DocAccessible* aDoc)
  : DocAccessibleChildBase(aDoc)
{
  MOZ_COUNT_CTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
  if (!sPlatformChild) {
    sPlatformChild = new PlatformChild();
    ClearOnShutdown(&sPlatformChild, ShutdownPhase::Shutdown);
  }
}

DocAccessibleChild::~DocAccessibleChild()
{
  MOZ_COUNT_DTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
}

void
DocAccessibleChild::SendCOMProxy(const IAccessibleHolder& aProxy)
{
  IAccessibleHolder parentProxy;
  uint32_t msaaID = AccessibleWrap::kNoID;
  PDocAccessibleChild::SendCOMProxy(aProxy, &parentProxy, &msaaID);
  mParentProxy.reset(parentProxy.Release());
  mDoc->SetID(msaaID);
}

} // namespace a11y
} // namespace mozilla

