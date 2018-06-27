/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChildProcessMessageManager_h
#define mozilla_dom_ChildProcessMessageManager_h

#include "mozilla/dom/SyncMessageSender.h"
#include "mozilla/dom/MessageManagerBinding.h"

namespace mozilla {
namespace dom {

class ChildProcessMessageManager final : public SyncMessageSender
{
public:
  explicit ChildProcessMessageManager(ipc::MessageManagerCallback* aCallback)
    : SyncMessageSender(aCallback,
                        MessageManagerFlags::MM_PROCESSMANAGER |
                        MessageManagerFlags::MM_OWNSCALLBACK)
  {
    mozilla::HoldJSObjects(this);
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return ChildProcessMessageManager_Binding::Wrap(aCx, this, aGivenProto);
  }

protected:
  virtual ~ChildProcessMessageManager()
  {
    mozilla::DropJSObjects(this);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChildProcessMessageManager_h
