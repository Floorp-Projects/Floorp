/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextMetrics_h
#define mozilla_dom_TextMetrics_h

#include "nsIDOMTextMetrics.h"

#define NS_TEXTMETRICSAZURE_PRIVATE_IID \
  {0x9793f9e7, 0x9dc1, 0x4e9c, {0x81, 0xc8, 0xfc, 0xa7, 0x14, 0xf4, 0x30, 0x79}}

namespace mozilla {
namespace dom {

class TextMetrics : public nsIDOMTextMetrics
{
public:
  TextMetrics(float w) : width(w) { }

  virtual ~TextMetrics() { }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TEXTMETRICSAZURE_PRIVATE_IID)

  NS_IMETHOD GetWidth(float* w)
  {
    *w = width;
    return NS_OK;
  }

  NS_DECL_ISUPPORTS

private:
  float width;
};

NS_IMPL_ADDREF(TextMetrics)
NS_IMPL_RELEASE(TextMetrics)

NS_INTERFACE_MAP_BEGIN(TextMetrics)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::TextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTextMetrics)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

} // namespace dom
} // namespace mozilla

DOMCI_DATA(TextMetrics, mozilla::dom::TextMetrics)

#endif // mozilla_dom_TextMetrics_h
