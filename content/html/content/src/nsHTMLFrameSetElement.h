/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsHTMLFrameSetElement_h__
#define nsHTMLFrameSetElement_h__

#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIFrameSetElement.h"
#include "nsGenericHTMLElement.h"

class nsHTMLFrameSetElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLFrameSetElement,
                              public nsIFrameSetElement
{
public:
  nsHTMLFrameSetElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLFrameSetElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLFrameSetElement
  NS_DECL_NSIDOMHTMLFRAMESETELEMENT

  // These override the SetAttr methods in nsGenericHTMLElement (need
  // both here to silence compiler warnings).
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

  // nsIFramesetElement
  NS_IMETHOD GetRowSpec(PRInt32 *aNumValues, const nsFramesetSpec** aSpecs);
  NS_IMETHOD GetColSpec(PRInt32 *aNumValues, const nsFramesetSpec** aSpecs);

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

private:
  nsresult ParseRowCol(const nsAString& aValue,
                       PRInt32&         aNumSpecs,
                       nsFramesetSpec** aSpecs);

  /**
   * The number of size specs in our "rows" attr
   */
  PRInt32          mNumRows;
  /**
   * The number of size specs in our "cols" attr
   */
  PRInt32          mNumCols;
  /**
   * The style hint to return for the rows/cols attrs in
   * GetAttributeChangeHint
   */
  nsChangeHint      mCurrentRowColHint;
  /**
   * The parsed representation of the "rows" attribute
   */
  nsAutoArrayPtr<nsFramesetSpec>  mRowSpecs; // parsed, non-computed dimensions
  /**
   * The parsed representation of the "cols" attribute
   */
  nsAutoArrayPtr<nsFramesetSpec>  mColSpecs; // parsed, non-computed dimensions
};

#endif
