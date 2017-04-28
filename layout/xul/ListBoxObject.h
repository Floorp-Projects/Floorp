/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ListBoxObject_h
#define mozilla_dom_ListBoxObject_h

#include "mozilla/dom/BoxObject.h"
#include "nsPIListBoxObject.h"

namespace mozilla {
namespace dom {

class ListBoxObject final : public BoxObject,
                            public nsPIListBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSILISTBOXOBJECT

  ListBoxObject();

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // nsPIListBoxObject
  virtual nsListBoxBodyFrame* GetListBoxBody(bool aFlush) override;

  // nsPIBoxObject
  virtual void Clear() override;
  virtual void ClearCachedValues() override;

  // ListBoxObject.webidl
  int32_t GetRowCount();
  int32_t GetRowHeight();
  int32_t GetNumberOfVisibleRows();
  int32_t GetIndexOfFirstVisibleRow();
  void EnsureIndexIsVisible(int32_t rowIndex);
  void ScrollToIndex(int32_t rowIndex);
  void ScrollByLines(int32_t numLines);
  already_AddRefed<Element> GetItemAtIndex(int32_t index);
  int32_t GetIndexOfItem(Element& item);

protected:
  nsListBoxBodyFrame *mListBoxBody;

private:
  ~ListBoxObject();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ListBoxObject_h
