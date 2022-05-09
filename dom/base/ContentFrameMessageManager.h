/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentFrameMessageManager_h
#define mozilla_dom_ContentFrameMessageManager_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/MessageManagerGlobal.h"
#include "nsContentUtils.h"
#include "xpcpublic.h"

namespace mozilla::dom {

template <typename>
struct Nullable;
class WindowProxyHolder;

#define NS_CONTENTFRAMEMESSAGEMANAGER_IID            \
  {                                                  \
    0x97e192a6, 0xab7a, 0x4c8f, {                    \
      0xb7, 0xdd, 0xf7, 0xec, 0x36, 0x38, 0x71, 0xb5 \
    }                                                \
  }

/**
 * Base class for implementing the WebIDL ContentFrameMessageManager class.
 */
class ContentFrameMessageManager : public DOMEventTargetHelper,
                                   public MessageManagerGlobal {
 public:
  using DOMEventTargetHelper::AddRef;
  using DOMEventTargetHelper::Release;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CONTENTFRAMEMESSAGEMANAGER_IID)

  virtual Nullable<WindowProxyHolder> GetContent(ErrorResult& aError) = 0;
  virtual already_AddRefed<nsIDocShell> GetDocShell(ErrorResult& aError) = 0;
  virtual already_AddRefed<nsIEventTarget> GetTabEventTarget() = 0;

  nsFrameMessageManager* GetMessageManager() { return mMessageManager; }
  void DisconnectMessageManager() {
    mMessageManager->Disconnect();
    mMessageManager = nullptr;
  }

  JSObject* GetOrCreateWrapper();

 protected:
  explicit ContentFrameMessageManager(nsFrameMessageManager* aMessageManager)
      : DOMEventTargetHelper(xpc::NativeGlobal(xpc::PrivilegedJunkScope())),
        MessageManagerGlobal(aMessageManager) {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(ContentFrameMessageManager,
                              NS_CONTENTFRAMEMESSAGEMANAGER_IID)

}  // namespace mozilla::dom

#endif  // mozilla_dom_ContentFrameMessageManager_h
