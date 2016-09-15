/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationTransportBuilderConstructor.h"

#include "nsComponentManagerUtils.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationSessionTransport.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(DummyPresentationTransportBuilderConstructor,
                  nsIPresentationTransportBuilderConstructor)

NS_IMETHODIMP
DummyPresentationTransportBuilderConstructor::CreateTransportBuilder(
                              uint8_t aType,
                              nsIPresentationSessionTransportBuilder** aRetval)
{
  MOZ_ASSERT(false, "Unexpected to be invoked.");
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(PresentationTransportBuilderConstructor,
                             DummyPresentationTransportBuilderConstructor)

/* static */ already_AddRefed<nsIPresentationTransportBuilderConstructor>
PresentationTransportBuilderConstructor::Create()
{
  nsCOMPtr<nsIPresentationTransportBuilderConstructor> constructor;
  if (XRE_IsContentProcess()) {
    constructor = new DummyPresentationTransportBuilderConstructor();
  } else {
    constructor = new PresentationTransportBuilderConstructor();
  }

  return constructor.forget();
}

NS_IMETHODIMP
PresentationTransportBuilderConstructor::CreateTransportBuilder(
                              uint8_t aType,
                              nsIPresentationSessionTransportBuilder** aRetval)
{
  if (NS_WARN_IF(!aRetval)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRetval = nullptr;

  if (NS_WARN_IF(aType != nsIPresentationChannelDescription::TYPE_TCP &&
                 aType != nsIPresentationChannelDescription::TYPE_DATACHANNEL)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (XRE_IsContentProcess()) {
    MOZ_ASSERT(false,
               "CreateTransportBuilder can only be invoked in parent process.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresentationSessionTransportBuilder> builder;
  if (aType == nsIPresentationChannelDescription::TYPE_TCP) {
    builder =
      do_CreateInstance(PRESENTATION_TCP_SESSION_TRANSPORT_CONTRACTID);
  } else {
    builder =
      do_CreateInstance("@mozilla.org/presentation/datachanneltransportbuilder;1");
  }

  if (NS_WARN_IF(!builder)) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  builder.forget(aRetval);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
