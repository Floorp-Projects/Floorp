/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Original Author:
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsAbView_H_
#define _nsAbView_H_

#include "nsISupports.h"
#include "nsIAbView.h"
#include "nsIOutlinerView.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerSelection.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIAbDirectory.h"
#include "nsIAtom.h"
#include "nsICollation.h"
#include "nsIAbListener.h"

typedef struct AbCard
{
  nsIAbCard *card;
  PRUnichar *primaryCollationKey;
  PRUnichar *secondaryCollationKey;
} AbCard;


class nsAbView : public nsIAbView, public nsIOutlinerView, public nsIAbListener
{
public:
  nsAbView();
  virtual ~nsAbView();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIABVIEW
  NS_DECL_NSIOUTLINERVIEW
  NS_DECL_NSIABLISTENER

private:
  nsCOMPtr<nsIOutlinerBoxObject> mOutliner;
  nsCOMPtr<nsIOutlinerSelection> mOutlinerSelection;
  nsresult SortBy(const PRUnichar *colID);
  nsresult CreateCollationKey(const PRUnichar *source,  PRUnichar **result);
  PRInt32 FindIndexForInsert(const PRUnichar *colID, AbCard *abcard);
  PRInt32 FindIndexForCard(const PRUnichar *colID, nsIAbCard *card);
  nsresult GenerateCollationKeysForCard(const PRUnichar *colID, AbCard *abcard);
  nsresult InvalidateOutliner(PRInt32 row);
  void RemoveCardAt(PRInt32 row);
  nsresult EnumerateCards();

  nsCString mURI;
  nsCOMPtr <nsIAbDirectory> mDirectory;
  nsVoidArray mCards;
  nsCOMPtr<nsIAtom> mMailListAtom;
  nsString mSortedColumn;
  nsCOMPtr<nsICollation> mCollationKeyGenerator;
};

#endif /* _nsAbView_H_ */
