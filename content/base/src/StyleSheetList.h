/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StyleSheetList_h
#define mozilla_dom_StyleSheetList_h

#include "nsIDOMStyleSheetList.h"

class nsCSSStyleSheet;

namespace mozilla {
namespace dom {

class StyleSheetList : public nsIDOMStyleSheetList
{
public:
  static StyleSheetList* FromSupports(nsISupports* aSupports)
  {
    nsIDOMStyleSheetList* list = static_cast<nsIDOMStyleSheetList*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMStyleSheetList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMStyleSheetList pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      MOZ_ASSERT(list_qi == list, "Uh, fix QI!");
    }
#endif
    return static_cast<StyleSheetList*>(list);
  }

  virtual nsCSSStyleSheet* GetItemAt(uint32_t aIndex) = 0;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StyleSheetList_h
