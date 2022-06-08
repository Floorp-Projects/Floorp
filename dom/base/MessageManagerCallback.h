/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_base_MessageManagerCallback_h__
#define dom_base_MessageManagerCallback_h__

#include "nsError.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class ClonedMessageData;
class ContentChild;
class ContentParent;
class ProcessMessageManager;

namespace ipc {

class StructuredCloneData;

class MessageManagerCallback {
 public:
  virtual ~MessageManagerCallback() = default;

  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) {
    return true;
  }

  virtual bool DoSendBlockingMessage(const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     nsTArray<StructuredCloneData>* aRetVal) {
    return true;
  }

  virtual nsresult DoSendAsyncMessage(const nsAString& aMessage,
                                      StructuredCloneData& aData) {
    return NS_OK;
  }

  virtual mozilla::dom::ProcessMessageManager* GetProcessMessageManager()
      const {
    return nullptr;
  }

  virtual void DoGetRemoteType(nsACString& aRemoteType,
                               ErrorResult& aError) const;

 protected:
  bool BuildClonedMessageData(StructuredCloneData& aData,
                              ClonedMessageData& aClonedData);
};

void UnpackClonedMessageData(const ClonedMessageData& aClonedData,
                             StructuredCloneData& aData);

}  // namespace ipc
}  // namespace dom
}  // namespace mozilla

#endif
