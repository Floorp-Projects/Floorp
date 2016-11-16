/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationBuilderChild_h
#define mozilla_dom_PresentationBuilderChild_h

#include "mozilla/dom/PPresentationBuilderChild.h"
#include "nsIPresentationSessionTransportBuilder.h"

namespace mozilla {
namespace dom {

class PresentationBuilderChild final: public PPresentationBuilderChild
                                    , public nsIPresentationSessionTransportBuilderListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTBUILDERLISTENER

  explicit PresentationBuilderChild(const nsString& aSessionId,
                                    uint8_t aRole);

  nsresult Init();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvOnOffer(const nsString& aSDP) override;

  virtual mozilla::ipc::IPCResult RecvOnAnswer(const nsString& aSDP) override;

  virtual mozilla::ipc::IPCResult RecvOnIceCandidate(const nsString& aCandidate) override;

private:
  virtual ~PresentationBuilderChild() = default;

  nsString mSessionId;
  uint8_t mRole;
  bool mActorDestroyed = false;
  nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder> mBuilder;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationBuilderChild_h
