/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequestEventTarget_h
#define mozilla_dom_XMLHttpRequestEventTarget_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla::dom {

class XMLHttpRequestEventTarget : public DOMEventTargetHelper {
 protected:
  explicit XMLHttpRequestEventTarget(DOMEventTargetHelper* aOwner)
      : DOMEventTargetHelper(aOwner) {}

  explicit XMLHttpRequestEventTarget(nsIGlobalObject* aGlobalObject)
      : DOMEventTargetHelper(aGlobalObject) {}

  virtual ~XMLHttpRequestEventTarget() = default;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XMLHttpRequestEventTarget,
                                           DOMEventTargetHelper)

  mozilla::Maybe<EventCallbackDebuggerNotificationType>
  GetDebuggerNotificationType() const override;

  IMPL_EVENT_HANDLER(loadstart)
  IMPL_EVENT_HANDLER(progress)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(load)
  IMPL_EVENT_HANDLER(timeout)
  IMPL_EVENT_HANDLER(loadend)

  nsISupports* GetParentObject() const { return GetOwner(); }

  virtual void DisconnectFromOwner() override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XMLHttpRequestEventTarget_h
