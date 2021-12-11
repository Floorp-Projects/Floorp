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

class HTMLDialogElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLDialogElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)),
        mPreviouslyFocusedElement(nullptr) {}

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLDialogElement, dialog)

  nsresult Clone(dom::NodeInfo* aNodeInfo, nsINode** aResult) const override;

  static bool IsDialogEnabled(JSContext* aCx, JS::Handle<JSObject*> aObj);

  bool Open() const { return GetBoolAttr(nsGkAtoms::open); }
  void SetOpen(bool aOpen, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::open, aOpen, aError);
  }

  void GetReturnValue(nsAString& aReturnValue) { aReturnValue = mReturnValue; }
  void SetReturnValue(const nsAString& aReturnValue) {
    mReturnValue = aReturnValue;
  }

  void UnbindFromTree(bool aNullParent = true) override;

  void Close(const mozilla::dom::Optional<nsAString>& aReturnValue);
  void Show();
  void ShowModal(ErrorResult& aError);

  bool IsInTopLayer() const;
  void QueueCancelDialog();
  void RunCancelDialogSteps();

  nsString mReturnValue;

 protected:
  virtual ~HTMLDialogElement();
  void FocusDialog();
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 private:
  void AddToTopLayerIfNeeded();
  void RemoveFromTopLayerIfNeeded();
  void StorePreviouslyFocusedElement();

  nsWeakPtr mPreviouslyFocusedElement;
};

}  // namespace dom
}  // namespace mozilla

#endif
