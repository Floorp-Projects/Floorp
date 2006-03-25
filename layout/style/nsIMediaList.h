/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * representation of media lists used when linking to style sheets or by
 * @media rules
 */

#ifndef nsIMediaList_h_
#define nsIMediaList_h_

#include "nsIDOMMediaList.h"
#include "nsAString.h"
#include "nsCOMArray.h"
#include "nsIAtom.h"
class nsPresContext;
class nsICSSStyleSheet;
class nsCSSStyleSheet;

class nsMediaList : public nsIDOMMediaList {
public:
  nsMediaList();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMMEDIALIST

  nsresult GetText(nsAString& aMediaText);
  nsresult SetText(const nsAString& aMediaText);
  PRBool Matches(nsPresContext* aPresContext);
  nsresult SetStyleSheet(nsICSSStyleSheet* aSheet);
  nsresult AppendAtom(nsIAtom* aMediumAtom) {
    return mArray.AppendObject(aMediumAtom) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult Clone(nsMediaList** aResult);

  PRInt32 Count() { return mArray.Count(); }
  nsIAtom* MediumAt(PRInt32 aIndex) { return mArray[aIndex]; }
  void Clear() { mArray.Clear(); }

protected:
  ~nsMediaList();

  nsresult Delete(const nsAString & aOldMedium);
  nsresult Append(const nsAString & aOldMedium);

  nsCOMArray<nsIAtom> mArray;
  // not refcounted; sheet will let us know when it goes away
  // mStyleSheet is the sheet that needs to be dirtied when this medialist
  // changes
  nsCSSStyleSheet*         mStyleSheet;
};
#endif /* !defined(nsIMediaList_h_) */
