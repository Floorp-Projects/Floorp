/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RadioNodeList_h
#define mozilla_dom_RadioNodeList_h

#include "nsContentList.h"
#include "nsCOMPtr.h"
#include "HTMLFormElement.h"
#include "mozilla/dom/BindingDeclarations.h"

#define MOZILLA_DOM_RADIONODELIST_IMPLEMENTATION_IID \
  { 0xbba7f3e8, 0xf3b5, 0x42e5, \
  { 0x82, 0x08, 0xa6, 0x8b, 0xe0, 0xbc, 0x22, 0x19 } }

namespace mozilla {
namespace dom {

class RadioNodeList : public nsSimpleContentList
{
public:
  explicit RadioNodeList(HTMLFormElement* aForm) : nsSimpleContentList(aForm) { }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;
  void GetValue(nsString& retval, CallerType aCallerType);
  void SetValue(const nsAString& value, CallerType aCallerType);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_RADIONODELIST_IMPLEMENTATION_IID)
private:
  ~RadioNodeList() { }
};

NS_DEFINE_STATIC_IID_ACCESSOR(RadioNodeList, MOZILLA_DOM_RADIONODELIST_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RadioNodeList_h
