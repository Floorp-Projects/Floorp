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
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFrameSetElement.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"

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
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

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

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;
private:
  nsresult ParseRowCol(const nsAString& aValue,
                       PRInt32&         aNumSpecs,
                       nsFramesetSpec** aSpecs);
  PRInt32 ParseRowColSpec(nsString&       aSpec, 
                          PRInt32         aMaxNumValues,
                          nsFramesetSpec* aSpecs);

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
  nsFramesetSpec*  mRowSpecs;  // parsed, non-computed dimensions
  /**
   * The parsed representation of the "cols" attribute
   */
  nsFramesetSpec*  mColSpecs;  // parsed, non-computed dimensions

  static PRInt32 gMaxNumRowColSpecs;
};

PRInt32 nsHTMLFrameSetElement::gMaxNumRowColSpecs = 25;


NS_IMPL_NS_NEW_HTML_ELEMENT(FrameSet)


nsHTMLFrameSetElement::nsHTMLFrameSetElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mNumRows(0), mNumCols(0),
    mCurrentRowColHint(NS_STYLE_HINT_REFLOW), mRowSpecs(nsnull),
    mColSpecs(nsnull)
{
}

nsHTMLFrameSetElement::~nsHTMLFrameSetElement()
{
  delete [] mRowSpecs;
  delete [] mColSpecs;
  mRowSpecs = mColSpecs = nsnull;
}


NS_IMPL_ADDREF_INHERITED(nsHTMLFrameSetElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLFrameSetElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLFrameSetElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFrameSetElement,
                                    nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFrameSetElement)
  NS_INTERFACE_MAP_ENTRY(nsIFrameSetElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFrameSetElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(FrameSet)


NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Cols, cols)
NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Rows, rows)

nsresult
nsHTMLFrameSetElement::SetAttr(PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               nsIAtom* aPrefix,
                               const nsAString& aValue,
                               PRBool aNotify)
{
  nsresult rv;
  /* The main goal here is to see whether the _number_ of rows or
   *  columns has changed.  If it has, we need to reframe; otherwise
   *  we want to reflow.  So we set mCurrentRowColHint here, then call
   *  nsGenericHTMLElement::SetAttr, which will end up calling
   *  GetAttributeChangeHint and notifying layout with that hint.
   *  Once nsGenericHTMLElement::SetAttr returns, we want to go back to our
   *  normal hint, which is NS_STYLE_HINT_REFLOW.
   */
  if (aAttribute == nsHTMLAtoms::rows && aNameSpaceID == kNameSpaceID_None) {
    PRInt32 oldRows = mNumRows;
    delete [] mRowSpecs;
    mRowSpecs = nsnull;
    mNumRows = 0;

    ParseRowCol(aValue, mNumRows, &mRowSpecs);
    
    if (mNumRows != oldRows) {
      mCurrentRowColHint = NS_STYLE_HINT_FRAMECHANGE;
    }
  } else if (aAttribute == nsHTMLAtoms::cols &&
             aNameSpaceID == kNameSpaceID_None) {
    PRInt32 oldCols = mNumCols;
    delete [] mColSpecs;
    mColSpecs = nsnull;
    mNumCols = 0;

    ParseRowCol(aValue, mNumCols, &mColSpecs);

    if (mNumCols != oldCols) {
      mCurrentRowColHint = NS_STYLE_HINT_FRAMECHANGE;
    }
  }
  
  rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aAttribute, aPrefix,
                                     aValue, aNotify);
  mCurrentRowColHint = NS_STYLE_HINT_REFLOW;
  
  return rv;
}

NS_IMETHODIMP
nsHTMLFrameSetElement::GetRowSpec(PRInt32 *aNumValues,
                                  const nsFramesetSpec** aSpecs)
{
  NS_PRECONDITION(aNumValues, "Must have a pointer to an integer here!");
  NS_PRECONDITION(aSpecs, "Must have a pointer to an array of nsFramesetSpecs");
  *aNumValues = 0;
  *aSpecs = nsnull;
  
  if (!mRowSpecs) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == GetHTMLAttribute(nsHTMLAtoms::rows, value) &&
        eHTMLUnit_String == value.GetUnit()) {
      nsAutoString rows;
      value.GetStringValue(rows);
      nsresult rv = ParseRowCol(rows, mNumRows, &mRowSpecs);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!mRowSpecs) {  // we may not have had an attr or had an empty attr
      mRowSpecs = new nsFramesetSpec[1];
      if (!mRowSpecs) {
        mNumRows = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mNumRows = 1;
      mRowSpecs[0].mUnit  = eFramesetUnit_Relative;
      mRowSpecs[0].mValue = 1;
    }
  }

  *aSpecs = mRowSpecs;
  *aNumValues = mNumRows;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameSetElement::GetColSpec(PRInt32 *aNumValues,
                                  const nsFramesetSpec** aSpecs)
{
  NS_PRECONDITION(aNumValues, "Must have a pointer to an integer here!");
  NS_PRECONDITION(aSpecs, "Must have a pointer to an array of nsFramesetSpecs");
  *aNumValues = 0;
  *aSpecs = nsnull;

  if (!mColSpecs) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == GetHTMLAttribute(nsHTMLAtoms::cols, value) &&
        eHTMLUnit_String == value.GetUnit()) {
      nsAutoString cols;
      value.GetStringValue(cols);
      nsresult rv = ParseRowCol(cols, mNumCols, &mColSpecs);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!mColSpecs) {  // we may not have had an attr or had an empty attr
      mColSpecs = new nsFramesetSpec[1];
      if (!mColSpecs) {
        mNumCols = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mNumCols = 1;
      mColSpecs[0].mUnit  = eFramesetUnit_Relative;
      mColSpecs[0].mValue = 1;
    }
  }

  *aSpecs = mColSpecs;
  *aNumValues = mNumCols;
  return NS_OK;
}


PRBool
nsHTMLFrameSetElement::ParseAttribute(nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::bordercolor) {
    return aResult.ParseColor(aValue, GetOwnerDoc());
  } 
  if (aAttribute == nsHTMLAtoms::frameborder) {
    return nsGenericHTMLElement::ParseFrameborderValue(aValue, aResult);
  } 
  if (aAttribute == nsHTMLAtoms::border) {
    return aResult.ParseIntWithBounds(aValue, 0, 100);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLFrameSetElement::AttributeToString(nsIAtom* aAttribute,
                                         const nsHTMLValue& aValue,
                                         nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::frameborder) {
    nsGenericHTMLElement::FrameborderValueToString(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  } 
  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLFrameSetElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType,
                                              nsChangeHint& aHint) const
{
  nsresult rv =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType, aHint);
  if (aAttribute == nsHTMLAtoms::rows ||
      aAttribute == nsHTMLAtoms::cols) {
    NS_UpdateHint(aHint, mCurrentRowColHint);
  }
  return rv;
}

nsresult
nsHTMLFrameSetElement::ParseRowCol(const nsAString & aValue,
                                   PRInt32& aNumSpecs,
                                   nsFramesetSpec** aSpecs) 
{
  NS_ASSERTION(!*aSpecs, "Someone called us with a pointer to an already allocated array of specs!");
  
  if (!aValue.IsEmpty()) {
    nsAutoString rowsCols(aValue);
    nsFramesetSpec* specs = new nsFramesetSpec[gMaxNumRowColSpecs];
    if (!specs) {
      *aSpecs = nsnull;
      aNumSpecs = 0;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aNumSpecs = ParseRowColSpec(rowsCols, gMaxNumRowColSpecs, specs);
    *aSpecs = new nsFramesetSpec[aNumSpecs];
    if (!*aSpecs) {
      aNumSpecs = 0;
      delete [] specs;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (PRInt32 i = 0; i < aNumSpecs; ++i) {
      (*aSpecs)[i] = specs[i];
    }
    delete [] specs;
  }

  return NS_OK;
}

/**
 * Translate a "rows" or "cols" spec into an array of nsFramesetSpecs
 */
PRInt32 
nsHTMLFrameSetElement::ParseRowColSpec(nsString&       aSpec, 
                                       PRInt32         aMaxNumValues, 
                                       nsFramesetSpec* aSpecs) 
{
  static const PRUnichar sAster('*');
  static const PRUnichar sPercent('%');
  static const PRUnichar sComma(',');

  // remove whitespace (Bug 33699) and quotation marks (bug 224598)
  // also remove leading/trailing commas (bug 31482)
  aSpec.StripChars(" \n\r\t\"\'");
  aSpec.Trim(",");
  
  // Count the commas 
  PRInt32 commaX = aSpec.FindChar(sComma);
  PRInt32 count = 1;
  while (commaX >= 0) {
    count++;
    commaX = aSpec.FindChar(sComma, commaX + 1);
  }

  if (count > aMaxNumValues) {
    NS_ASSERTION(0, "Not enough space for values");
    count = aMaxNumValues;
  }

  // Parse each comma separated token

  PRInt32 start = 0;
  PRInt32 specLen = aSpec.Length();

  for (PRInt32 i = 0; i < count; i++) {
    // Find our comma
    commaX = aSpec.FindChar(sComma, start);
    PRInt32 end = (commaX < 0) ? specLen : commaX;

    // Note: If end == start then it means that the token has no
    // data in it other than a terminating comma (or the end of the spec)
    aSpecs[i].mUnit = eFramesetUnit_Fixed;
    if (end > start) {
      PRInt32 numberEnd = end;
      PRUnichar ch = aSpec.CharAt(numberEnd - 1);
      if (sAster == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Relative;
        numberEnd--;
      } else if (sPercent == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Percent;
        numberEnd--;
        // check for "*%"
        if (numberEnd > start) {
          ch = aSpec.CharAt(numberEnd - 1);
          if (sAster == ch) {
            aSpecs[i].mUnit = eFramesetUnit_Relative;
            numberEnd--;
          }
        }
      }

      // Translate value to an integer
      nsString token;
      aSpec.Mid(token, start, numberEnd - start);

      // Treat * as 1*
      if ((eFramesetUnit_Relative == aSpecs[i].mUnit) &&
        (0 == token.Length())) {
        aSpecs[i].mValue = 1;
      }
      else {
        // Otherwise just convert to integer.
        PRInt32 err;
        aSpecs[i].mValue = token.ToInteger(&err);
        if (err) {
          aSpecs[i].mValue = 0;
        }
      }

      // Treat 0* as 1* in quirks mode (bug 40383)
      nsCompatibility mode = eCompatibility_FullStandards;
      nsCOMPtr<nsIHTMLDocument> htmlDocument =
        do_QueryInterface(GetOwnerDoc());
      if (htmlDocument) {
        mode = htmlDocument->GetCompatibilityMode();
      }
      
      if (eCompatibility_NavQuirks == mode) {
        if ((eFramesetUnit_Relative == aSpecs[i].mUnit) &&
          (0 == aSpecs[i].mValue)) {
          aSpecs[i].mValue = 1;
        }
      }
        
      // Catch zero and negative frame sizes for Nav compatability
      // Nav resized absolute and relative frames to "1" and
      // percent frames to an even percentage of the width
      //
      //if ((eCompatibility_NavQuirks == aMode) && (aSpecs[i].mValue <= 0)) {
      //  if (eFramesetUnit_Percent == aSpecs[i].mUnit) {
      //    aSpecs[i].mValue = 100 / count;
      //  } else {
      //    aSpecs[i].mValue = 1;
      //  }
      //} else {

      // In standards mode, just set negative sizes to zero
      if (aSpecs[i].mValue < 0) {
        aSpecs[i].mValue = 0;
      }
      start = end + 1;
    }
  }
  return count;
}

