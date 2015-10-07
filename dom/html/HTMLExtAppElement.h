/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLExtAppElement_h
#define mozilla_dom_HTMLExtAppElement_h

#include "nsGenericHTMLElement.h"
#include "nsIExternalApplication.h"

class nsCustomEventDispatch;
class nsCustomPropertyBag;

namespace mozilla {
namespace dom {

class HTMLExtAppElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLExtAppElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLExtAppElement,
                                           nsGenericHTMLElement)

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  void GetCustomProperty(const nsAString& aName, nsString& aReturn);
  void PostMessage(const nsAString& aMessage, ErrorResult& aRv);

protected:
  virtual ~HTMLExtAppElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsRefPtr<nsCustomEventDispatch> mCustomEventDispatch;
  nsRefPtr<nsCustomPropertyBag> mCustomPropertyBag;
  nsCOMPtr<nsIExternalApplication> mApp;
};

} // namespace dom
} // namespace mozilla

class nsCustomEventDispatch final : public nsICustomEventDispatch
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICUSTOMEVENTDISPATCH

  explicit nsCustomEventDispatch(mozilla::dom::EventTarget* aEventTarget);
  void ClearEventTarget();

private:
  ~nsCustomEventDispatch();

  // Weak pointer, this object is owned by the event target.
  mozilla::dom::EventTarget* mEventTarget;
};

class nsCustomPropertyBag final : public nsICustomPropertyBag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICUSTOMPROPERTYBAG

  nsCustomPropertyBag();
  void GetCustomProperty(const nsAString& aName, nsString& aReturn);

private:
  ~nsCustomPropertyBag();

  nsClassHashtable<nsStringHashKey, nsString> mBag;
};

#endif // mozilla_dom_HTMLExtAppElement_h
