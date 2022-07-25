/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EndpointForReportParent_h
#define mozilla_dom_EndpointForReportParent_h

#include "mozilla/dom/PEndpointForReportParent.h"

namespace mozilla {
namespace ipc {
class PrincipalInfo;
}

namespace dom {

class EndpointForReport;

class EndpointForReportParent final : public PEndpointForReportParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EndpointForReportParent)

  EndpointForReportParent();

  void Run(const nsAString& aGroupName,
           const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

 private:
  ~EndpointForReportParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsCOMPtr<nsIThread> mPBackgroundThread;
  bool mActive;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EndpointForReportParent_h
