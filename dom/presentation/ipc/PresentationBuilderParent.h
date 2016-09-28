/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationBuilderParent_h__
#define mozilla_dom_PresentationBuilderParent_h__

#include "mozilla/dom/PPresentationBuilderParent.h"
#include "PresentationParent.h"
#include "nsIPresentationSessionTransportBuilder.h"

namespace mozilla {
namespace dom {

class PresentationBuilderParent final: public PPresentationBuilderParent
                                     , public nsIPresentationDataChannelSessionTransportBuilder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTBUILDER
  NS_DECL_NSIPRESENTATIONDATACHANNELSESSIONTRANSPORTBUILDER

  explicit PresentationBuilderParent(PresentationParent* aParent);

  virtual bool RecvSendOffer(const nsString& aSDP) override;

  virtual bool RecvSendAnswer(const nsString& aSDP) override;

  virtual bool RecvSendIceCandidate(const nsString& aCandidate) override;

  virtual bool RecvClose(const nsresult& aReason) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvOnSessionTransport() override;

  virtual bool RecvOnSessionTransportError(const nsresult& aReason) override;

private:
  virtual ~PresentationBuilderParent();
  bool mNeedDestroyActor = false;
  RefPtr<PresentationParent> mParent;
  nsCOMPtr<nsIPresentationSessionTransportBuilderListener> mBuilderListener;
  nsCOMPtr<nsIPresentationSessionTransport> mIPCSessionTransport;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationBuilderParent_h__
