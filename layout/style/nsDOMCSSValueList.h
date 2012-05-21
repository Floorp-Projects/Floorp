/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing lists of values in DOM computed style */

#ifndef nsDOMCSSValueList_h___
#define nsDOMCSSValueList_h___

#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSValueList.h"
#include "nsTArray.h"


#include "nsCOMPtr.h"

class nsDOMCSSValueList : public nsIDOMCSSValueList
{
public:
  NS_DECL_ISUPPORTS

  // nsIDOMCSSValueList
  NS_DECL_NSIDOMCSSVALUELIST

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // nsDOMCSSValueList
  nsDOMCSSValueList(bool aCommaDelimited, bool aReadonly);
  virtual ~nsDOMCSSValueList();

  /**
   * Adds a value to this list.
   */
  void AppendCSSValue(nsIDOMCSSValue* aValue);

  nsIDOMCSSValue* GetItemAt(PRUint32 aIndex)
  {
    return mCSSValues.SafeElementAt(aIndex, nsnull);
  }

  static nsDOMCSSValueList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMCSSValueList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMCSSValueList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMCSSValueList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMCSSValueList*>(aSupports);
  }

private:
  bool                        mCommaDelimited;  // some value lists use a comma
                                                // as the delimiter, some just use
                                                // spaces.

  bool                        mReadonly;    // Are we read-only?

  InfallibleTArray<nsCOMPtr<nsIDOMCSSValue> > mCSSValues;
};


#endif /* nsDOMCSSValueList_h___ */
