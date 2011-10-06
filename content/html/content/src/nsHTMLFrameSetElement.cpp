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

#include "nsHTMLFrameSetElement.h"
#include "jsapi.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(FrameSet)


nsHTMLFrameSetElement::nsHTMLFrameSetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mNumRows(0), mNumCols(0),
    mCurrentRowColHint(NS_STYLE_HINT_REFLOW)
{
}

nsHTMLFrameSetElement::~nsHTMLFrameSetElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLFrameSetElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLFrameSetElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLFrameSetElement, nsHTMLFrameSetElement)

// QueryInterface implementation for nsHTMLFrameSetElement
NS_INTERFACE_TABLE_HEAD(nsHTMLFrameSetElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLFrameSetElement,
                                   nsIDOMHTMLFrameSetElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLFrameSetElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLFrameSetElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLFrameSetElement)


NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Cols, cols)
NS_IMPL_STRING_ATTR(nsHTMLFrameSetElement, Rows, rows)

nsresult
nsHTMLFrameSetElement::SetAttr(PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               nsIAtom* aPrefix,
                               const nsAString& aValue,
                               bool aNotify)
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
  if (aAttribute == nsGkAtoms::rows && aNameSpaceID == kNameSpaceID_None) {
    PRInt32 oldRows = mNumRows;
    ParseRowCol(aValue, mNumRows, getter_Transfers(mRowSpecs));
    
    if (mNumRows != oldRows) {
      mCurrentRowColHint = NS_STYLE_HINT_FRAMECHANGE;
    }
  } else if (aAttribute == nsGkAtoms::cols &&
             aNameSpaceID == kNameSpaceID_None) {
    PRInt32 oldCols = mNumCols;
    ParseRowCol(aValue, mNumCols, getter_Transfers(mColSpecs));

    if (mNumCols != oldCols) {
      mCurrentRowColHint = NS_STYLE_HINT_FRAMECHANGE;
    }
  }
  
  rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aAttribute, aPrefix,
                                     aValue, aNotify);
  mCurrentRowColHint = NS_STYLE_HINT_REFLOW;
  
  return rv;
}

nsresult
nsHTMLFrameSetElement::GetRowSpec(PRInt32 *aNumValues,
                                  const nsFramesetSpec** aSpecs)
{
  NS_PRECONDITION(aNumValues, "Must have a pointer to an integer here!");
  NS_PRECONDITION(aSpecs, "Must have a pointer to an array of nsFramesetSpecs");
  *aNumValues = 0;
  *aSpecs = nsnull;
  
  if (!mRowSpecs) {
    const nsAttrValue* value = GetParsedAttr(nsGkAtoms::rows);
    if (value && value->Type() == nsAttrValue::eString) {
      nsresult rv = ParseRowCol(value->GetStringValue(), mNumRows,
                                getter_Transfers(mRowSpecs));
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

nsresult
nsHTMLFrameSetElement::GetColSpec(PRInt32 *aNumValues,
                                  const nsFramesetSpec** aSpecs)
{
  NS_PRECONDITION(aNumValues, "Must have a pointer to an integer here!");
  NS_PRECONDITION(aSpecs, "Must have a pointer to an array of nsFramesetSpecs");
  *aNumValues = 0;
  *aSpecs = nsnull;

  if (!mColSpecs) {
    const nsAttrValue* value = GetParsedAttr(nsGkAtoms::cols);
    if (value && value->Type() == nsAttrValue::eString) {
      nsresult rv = ParseRowCol(value->GetStringValue(), mNumCols,
                                getter_Transfers(mColSpecs));
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


bool
nsHTMLFrameSetElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::bordercolor) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::frameborder) {
      return nsGenericHTMLElement::ParseFrameborderValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::border) {
      return aResult.ParseIntWithBounds(aValue, 0, 100);
    }
  }
  
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsChangeHint
nsHTMLFrameSetElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::rows ||
      aAttribute == nsGkAtoms::cols) {
    NS_UpdateHint(retval, mCurrentRowColHint);
  }
  return retval;
}

/**
 * Translate a "rows" or "cols" spec into an array of nsFramesetSpecs
 */
nsresult
nsHTMLFrameSetElement::ParseRowCol(const nsAString & aValue,
                                   PRInt32& aNumSpecs,
                                   nsFramesetSpec** aSpecs) 
{
  if (aValue.IsEmpty()) {
    aNumSpecs = 0;
    *aSpecs = nsnull;
    return NS_OK;
  }

  static const PRUnichar sAster('*');
  static const PRUnichar sPercent('%');
  static const PRUnichar sComma(',');

  nsAutoString spec(aValue);
  // remove whitespace (Bug 33699) and quotation marks (bug 224598)
  // also remove leading/trailing commas (bug 31482)
  spec.StripChars(" \n\r\t\"\'");
  spec.Trim(",");
  
  // Count the commas. Don't count more than X commas (bug 576447).
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT * sizeof(nsFramesetSpec) < (1 << 30));
  PRInt32 commaX = spec.FindChar(sComma);
  PRInt32 count = 1;
  while (commaX != kNotFound && count < NS_MAX_FRAMESET_SPEC_COUNT) {
    count++;
    commaX = spec.FindChar(sComma, commaX + 1);
  }

  nsFramesetSpec* specs = new nsFramesetSpec[count];
  if (!specs) {
    *aSpecs = nsnull;
    aNumSpecs = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Pre-grab the compat mode; we may need it later in the loop.
  bool isInQuirks = InNavQuirksMode(GetOwnerDoc());
      
  // Parse each comma separated token

  PRInt32 start = 0;
  PRInt32 specLen = spec.Length();

  for (PRInt32 i = 0; i < count; i++) {
    // Find our comma
    commaX = spec.FindChar(sComma, start);
    NS_ASSERTION(i == count - 1 || commaX != kNotFound,
                 "Failed to find comma, somehow");
    PRInt32 end = (commaX == kNotFound) ? specLen : commaX;

    // Note: If end == start then it means that the token has no
    // data in it other than a terminating comma (or the end of the spec).
    // So default to a fixed width of 0.
    specs[i].mUnit = eFramesetUnit_Fixed;
    specs[i].mValue = 0;
    if (end > start) {
      PRInt32 numberEnd = end;
      PRUnichar ch = spec.CharAt(numberEnd - 1);
      if (sAster == ch) {
        specs[i].mUnit = eFramesetUnit_Relative;
        numberEnd--;
      } else if (sPercent == ch) {
        specs[i].mUnit = eFramesetUnit_Percent;
        numberEnd--;
        // check for "*%"
        if (numberEnd > start) {
          ch = spec.CharAt(numberEnd - 1);
          if (sAster == ch) {
            specs[i].mUnit = eFramesetUnit_Relative;
            numberEnd--;
          }
        }
      }

      // Translate value to an integer
      nsAutoString token;
      spec.Mid(token, start, numberEnd - start);

      // Treat * as 1*
      if ((eFramesetUnit_Relative == specs[i].mUnit) &&
        (0 == token.Length())) {
        specs[i].mValue = 1;
      }
      else {
        // Otherwise just convert to integer.
        PRInt32 err;
        specs[i].mValue = token.ToInteger(&err);
        if (err) {
          specs[i].mValue = 0;
        }
      }

      // Treat 0* as 1* in quirks mode (bug 40383)
      if (isInQuirks) {
        if ((eFramesetUnit_Relative == specs[i].mUnit) &&
          (0 == specs[i].mValue)) {
          specs[i].mValue = 1;
        }
      }
        
      // Catch zero and negative frame sizes for Nav compatibility
      // Nav resized absolute and relative frames to "1" and
      // percent frames to an even percentage of the width
      //
      //if (isInQuirks && (specs[i].mValue <= 0)) {
      //  if (eFramesetUnit_Percent == specs[i].mUnit) {
      //    specs[i].mValue = 100 / count;
      //  } else {
      //    specs[i].mValue = 1;
      //  }
      //} else {

      // In standards mode, just set negative sizes to zero
      if (specs[i].mValue < 0) {
        specs[i].mValue = 0;
      }
      start = end + 1;
    }
  }

  aNumSpecs = count;
  // Transfer ownership to caller here
  *aSpecs = specs;
  
  return NS_OK;
}

// Event listener stuff
// FIXME (https://bugzilla.mozilla.org/show_bug.cgi?id=431767)
// nsDocument::GetInnerWindow can return an outer window in some
// cases.  We don't want to stick an event listener on an outer
// window, so bail if it does.  See also similar code in
// nsGenericHTMLElement::GetEventListenerManagerForAttr.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the shim */
#define FORWARDED_EVENT(name_, id_, type_, struct_)                   \
  NS_IMETHODIMP nsHTMLFrameSetElement::GetOn##name_(JSContext *cx,    \
                                               jsval *vp) {           \
    /* XXXbz note to self: add tests for this! */                     \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();             \
    if (win && win->IsInnerWindow()) {                                \
      nsCOMPtr<nsIInlineEventHandlers> ev = do_QueryInterface(win);   \
      return ev->GetOn##name_(cx, vp);                                \
    }                                                                 \
    *vp = JSVAL_NULL;                                                 \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP nsHTMLFrameSetElement::SetOn##name_(JSContext *cx,    \
                                               const jsval &v) {      \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();             \
    if (win && win->IsInnerWindow()) {                                \
      nsCOMPtr<nsIInlineEventHandlers> ev = do_QueryInterface(win);   \
      return ev->SetOn##name_(cx, v);                                 \
    }                                                                 \
    return NS_OK;                                                     \
  }
#define WINDOW_EVENT(name_, id_, type_, struct_)                      \
  NS_IMETHODIMP nsHTMLFrameSetElement::GetOn##name_(JSContext *cx,    \
                                                    jsval *vp) {      \
    /* XXXbz note to self: add tests for this! */                     \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();             \
    if (win && win->IsInnerWindow()) {                                \
      return win->GetOn##name_(cx, vp);                               \
    }                                                                 \
    *vp = JSVAL_NULL;                                                 \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP nsHTMLFrameSetElement::SetOn##name_(JSContext *cx,    \
                                                    const jsval &v) { \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();             \
    if (win && win->IsInnerWindow()) {                                \
      return win->SetOn##name_(cx, v);                                \
    }                                                                 \
    return NS_OK;                                                     \
  }
#include "nsEventNameList.h"
#undef WINDOW_EVENT
#undef FORWARDED_EVENT
#undef EVENT
