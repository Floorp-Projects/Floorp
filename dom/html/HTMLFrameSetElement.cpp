/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFrameSetElement.h"
#include "mozilla/dom/HTMLFrameSetElementBinding.h"
#include "mozilla/dom/EventHandlerBinding.h"
#include "nsGlobalWindow.h"
#include "mozilla/UniquePtrExtensions.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(FrameSet)

namespace mozilla {
namespace dom {

HTMLFrameSetElement::~HTMLFrameSetElement()
{
}

JSObject*
HTMLFrameSetElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLFrameSetElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLFrameSetElement, nsGenericHTMLElement,
                            nsIDOMHTMLFrameSetElement)

NS_IMPL_ELEMENT_CLONE(HTMLFrameSetElement)

NS_IMETHODIMP 
HTMLFrameSetElement::SetCols(const nsAString& aCols)
{
  ErrorResult rv;
  SetCols(aCols, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLFrameSetElement::GetCols(nsAString& aCols)
{
  DOMString cols;
  GetCols(cols);
  cols.ToString(aCols);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLFrameSetElement::SetRows(const nsAString& aRows)
{
  ErrorResult rv;
  SetRows(aRows, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLFrameSetElement::GetRows(nsAString& aRows)
{
  DOMString rows;
  GetRows(rows);
  rows.ToString(aRows);
  return NS_OK;
}

nsresult
HTMLFrameSetElement::SetAttr(int32_t aNameSpaceID,
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
    int32_t oldRows = mNumRows;
    ParseRowCol(aValue, mNumRows, &mRowSpecs);

    if (mNumRows != oldRows) {
      mCurrentRowColHint = nsChangeHint_ReconstructFrame;
    }
  } else if (aAttribute == nsGkAtoms::cols &&
             aNameSpaceID == kNameSpaceID_None) {
    int32_t oldCols = mNumCols;
    ParseRowCol(aValue, mNumCols, &mColSpecs);

    if (mNumCols != oldCols) {
      mCurrentRowColHint = nsChangeHint_ReconstructFrame;
    }
  }

  rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aAttribute, aPrefix,
                                     aValue, aNotify);
  mCurrentRowColHint = NS_STYLE_HINT_REFLOW;

  return rv;
}

nsresult
HTMLFrameSetElement::GetRowSpec(int32_t *aNumValues,
                                const nsFramesetSpec** aSpecs)
{
  NS_PRECONDITION(aNumValues, "Must have a pointer to an integer here!");
  NS_PRECONDITION(aSpecs, "Must have a pointer to an array of nsFramesetSpecs");
  *aNumValues = 0;
  *aSpecs = nullptr;
  
  if (!mRowSpecs) {
    const nsAttrValue* value = GetParsedAttr(nsGkAtoms::rows);
    if (value && value->Type() == nsAttrValue::eString) {
      nsresult rv = ParseRowCol(value->GetStringValue(), mNumRows,
                                &mRowSpecs);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!mRowSpecs) {  // we may not have had an attr or had an empty attr
      mRowSpecs = MakeUnique<nsFramesetSpec[]>(1);
      mNumRows = 1;
      mRowSpecs[0].mUnit  = eFramesetUnit_Relative;
      mRowSpecs[0].mValue = 1;
    }
  }

  *aSpecs = mRowSpecs.get();
  *aNumValues = mNumRows;
  return NS_OK;
}

nsresult
HTMLFrameSetElement::GetColSpec(int32_t *aNumValues,
                                const nsFramesetSpec** aSpecs)
{
  NS_PRECONDITION(aNumValues, "Must have a pointer to an integer here!");
  NS_PRECONDITION(aSpecs, "Must have a pointer to an array of nsFramesetSpecs");
  *aNumValues = 0;
  *aSpecs = nullptr;

  if (!mColSpecs) {
    const nsAttrValue* value = GetParsedAttr(nsGkAtoms::cols);
    if (value && value->Type() == nsAttrValue::eString) {
      nsresult rv = ParseRowCol(value->GetStringValue(), mNumCols,
                                &mColSpecs);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!mColSpecs) {  // we may not have had an attr or had an empty attr
      mColSpecs = MakeUnique<nsFramesetSpec[]>(1);
      mNumCols = 1;
      mColSpecs[0].mUnit  = eFramesetUnit_Relative;
      mColSpecs[0].mValue = 1;
    }
  }

  *aSpecs = mColSpecs.get();
  *aNumValues = mNumCols;
  return NS_OK;
}


bool
HTMLFrameSetElement::ParseAttribute(int32_t aNamespaceID,
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
HTMLFrameSetElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            int32_t aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::rows ||
      aAttribute == nsGkAtoms::cols) {
    retval |= mCurrentRowColHint;
  }
  return retval;
}

/**
 * Translate a "rows" or "cols" spec into an array of nsFramesetSpecs
 */
nsresult
HTMLFrameSetElement::ParseRowCol(const nsAString & aValue,
                                 int32_t& aNumSpecs,
                                 UniquePtr<nsFramesetSpec[]>* aSpecs)
{
  if (aValue.IsEmpty()) {
    aNumSpecs = 0;
    *aSpecs = nullptr;
    return NS_OK;
  }

  static const char16_t sAster('*');
  static const char16_t sPercent('%');
  static const char16_t sComma(',');

  nsAutoString spec(aValue);
  // remove whitespace (Bug 33699) and quotation marks (bug 224598)
  // also remove leading/trailing commas (bug 31482)
  spec.StripChars(" \n\r\t\"\'");
  spec.Trim(",");
  
  // Count the commas. Don't count more than X commas (bug 576447).
  static_assert(NS_MAX_FRAMESET_SPEC_COUNT * sizeof(nsFramesetSpec) < (1 << 30),
                "Too many frameset specs allowed to allocate");
  int32_t commaX = spec.FindChar(sComma);
  int32_t count = 1;
  while (commaX != kNotFound && count < NS_MAX_FRAMESET_SPEC_COUNT) {
    count++;
    commaX = spec.FindChar(sComma, commaX + 1);
  }

  auto specs = MakeUniqueFallible<nsFramesetSpec[]>(count);
  if (!specs) {
    *aSpecs = nullptr;
    aNumSpecs = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Pre-grab the compat mode; we may need it later in the loop.
  bool isInQuirks = InNavQuirksMode(OwnerDoc());
      
  // Parse each comma separated token

  int32_t start = 0;
  int32_t specLen = spec.Length();

  for (int32_t i = 0; i < count; i++) {
    // Find our comma
    commaX = spec.FindChar(sComma, start);
    NS_ASSERTION(i == count - 1 || commaX != kNotFound,
                 "Failed to find comma, somehow");
    int32_t end = (commaX == kNotFound) ? specLen : commaX;

    // Note: If end == start then it means that the token has no
    // data in it other than a terminating comma (or the end of the spec).
    // So default to a fixed width of 0.
    specs[i].mUnit = eFramesetUnit_Fixed;
    specs[i].mValue = 0;
    if (end > start) {
      int32_t numberEnd = end;
      char16_t ch = spec.CharAt(numberEnd - 1);
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
        nsresult err;
        specs[i].mValue = token.ToInteger(&err);
        if (NS_FAILED(err)) {
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
  *aSpecs = Move(specs);

  return NS_OK;
}

bool
HTMLFrameSetElement::IsEventAttributeName(nsIAtom *aName)
{
  return nsContentUtils::IsEventAttributeName(aName,
                                              EventNameType_HTML |
                                              EventNameType_HTMLBodyOrFramesetOnly);
}


#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the shim */
// nsGenericHTMLElement::GetOnError returns
// already_AddRefed<EventHandlerNonNull> while other getters return
// EventHandlerNonNull*, so allow passing in the type to use here.
#define WINDOW_EVENT_HELPER(name_, type_)                                      \
  type_*                                                                       \
  HTMLFrameSetElement::GetOn##name_()                                          \
  {                                                                            \
    if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {              \
      nsGlobalWindow* globalWin = nsGlobalWindow::Cast(win);                   \
      return globalWin->GetOn##name_();                                        \
    }                                                                          \
    return nullptr;                                                            \
  }                                                                            \
  void                                                                         \
  HTMLFrameSetElement::SetOn##name_(type_* handler)                            \
  {                                                                            \
    nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();                    \
    if (!win) {                                                                \
      return;                                                                  \
    }                                                                          \
                                                                               \
    nsGlobalWindow* globalWin = nsGlobalWindow::Cast(win);                     \
    return globalWin->SetOn##name_(handler);                                   \
  }
#define WINDOW_EVENT(name_, id_, type_, struct_)                               \
  WINDOW_EVENT_HELPER(name_, EventHandlerNonNull)
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                         \
  WINDOW_EVENT_HELPER(name_, OnBeforeUnloadEventHandlerNonNull)
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef BEFOREUNLOAD_EVENT
#undef WINDOW_EVENT
#undef WINDOW_EVENT_HELPER
#undef EVENT

} // namespace dom
} // namespace mozilla
