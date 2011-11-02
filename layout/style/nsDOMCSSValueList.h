/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
