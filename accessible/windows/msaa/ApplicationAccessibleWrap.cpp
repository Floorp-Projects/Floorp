/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationAccessibleWrap.h"

#include "nsIGfxInfo.h"
#include "AccAttributes.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Components.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports
NS_IMPL_ISUPPORTS_INHERITED0(ApplicationAccessibleWrap, ApplicationAccessible)

already_AddRefed<AccAttributes> ApplicationAccessibleWrap::NativeAttributes() {
  RefPtr<AccAttributes> attributes = new AccAttributes();

  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
  if (gfxInfo) {
    bool isD2DEnabled = false;
    gfxInfo->GetD2DEnabled(&isD2DEnabled);
    RefPtr<nsAtom> attrName = NS_Atomize(u"D2D"_ns);
    attributes->SetAttribute(attrName, isD2DEnabled);
  }

  return attributes.forget();
}

void ApplicationAccessibleWrap::Shutdown() {
  // ApplicationAccessible::Shutdown doesn't call AccessibleWrap::Shutdown, so
  // we have to call MsaaShutdown here.
  if (mMsaa) {
    mMsaa->MsaaShutdown();
  }
  ApplicationAccessible::Shutdown();
}
