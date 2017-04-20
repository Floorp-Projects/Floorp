/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLDialogElement_h
#define HTMLDialogElement_h

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class HTMLDialogElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLDialogElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo) : nsGenericHTMLElement(aNodeInfo)
  {
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLDialogElement, dialog)

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;

  static bool IsDialogEnabled();

  bool Open() const { return GetBoolAttr(nsGkAtoms::open); }
  void SetOpen(bool aOpen, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::open, aOpen, aError);
  }

  void GetReturnValue(nsAString& aReturnValue)
  {
    aReturnValue = mReturnValue;
  }
  void SetReturnValue(const nsAString& aReturnValue)
  {
    mReturnValue = aReturnValue;
  }

  void Close(const mozilla::dom::Optional<nsAString>& aReturnValue);
  void Show();
  void ShowModal(ErrorResult& aError);

  nsString mReturnValue;

protected:
  virtual ~HTMLDialogElement();
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif
