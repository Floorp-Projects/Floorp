/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Author: Aaron Leventhal (aaronl@netscape.com)
 *
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

#ifndef _nsHTMLTableAccessible_H_
#define _nsHTMLTableAccessible_H_

#include "nsAccessible.h"
#include "nsBaseWidgetAccessible.h"
#include "nsIAccessibleTable.h"
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLCollection.h"
#include "nsITableLayout.h"
#include "nsHyperTextAccessible.h"

#ifndef MOZ_ACCESSIBILITY_ATK
class nsHTMLTableCellAccessible : public nsBlockAccessible
#else
class nsHTMLTableCellAccessible : public nsBlockAccessible, public nsAccessibleHyperText
#endif
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsHTMLTableCellAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetAccRole(PRUint32 *aResult); 
  NS_IMETHOD GetAccState(PRUint32 *aResult); 
};

class nsHTMLTableCaptionAccessible : public nsAccessible
{
public:
  nsHTMLTableCaptionAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetAccState(PRUint32 *aResult);
  NS_IMETHOD GetAccValue(nsAString& aResult);
};

#ifndef MOZ_ACCESSIBILITY_ATK
class nsHTMLTableAccessible : public nsBlockAccessible
#else
class nsHTMLTableAccessible : public nsBlockAccessible,
                              public nsIAccessibleTable
#endif
{
public:
  NS_DECL_ISUPPORTS_INHERITED
#ifdef MOZ_ACCESSIBILITY_ATK
  NS_DECL_NSIACCESSIBLETABLE
#endif

  nsHTMLTableAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  /* nsIAccessible */
  NS_IMETHOD GetAccRole(PRUint32 *aResult); 
  NS_IMETHOD GetAccState(PRUint32 *aResult); 
  NS_IMETHOD GetAccName(nsAString& aResult);

protected:
#ifdef MOZ_ACCESSIBILITY_ATK
  nsresult GetTableNode(nsIDOMNode **_retval);
  nsresult GetTableLayout(nsITableLayout **aLayoutObject);
  nsresult GetCellAt(PRInt32        aRowIndex,
                     PRInt32        aColIndex,
                     nsIDOMElement* &aCell);
#endif
};

#ifdef MOZ_ACCESSIBILITY_ATK
class nsHTMLTableHeadAccessible : public nsHTMLTableAccessible
{
public:
  nsHTMLTableHeadAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell);

  /* nsIAccessible */
  NS_IMETHOD GetAccRole(PRUint32 *aResult);

  /* nsIAccessibleTable */
  NS_IMETHOD GetCaption(nsIAccessible **aCaption);
  NS_IMETHOD SetCaption(nsIAccessible *aCaption);
  NS_IMETHOD GetSummary(nsAString &aSummary);
  NS_IMETHOD SetSummary(const nsAString &aSummary);
  NS_IMETHOD GetColumnHeader(nsIAccessibleTable **aColumnHeader);
  NS_IMETHOD GetRows(PRInt32 *aRows);
};

#endif //MOZ_ACCESSIBILITY_ATK

#endif  
