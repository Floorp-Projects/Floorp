/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLFrameSetElement_h
#define HTMLFrameSetElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsGenericHTMLElement.h"

/**
 * The nsFramesetUnit enum is used to denote the type of each entry
 * in the row or column spec.
 */
enum nsFramesetUnit {
  eFramesetUnit_Fixed = 0,
  eFramesetUnit_Percent,
  eFramesetUnit_Relative
};

/**
 * The nsFramesetSpec struct is used to hold a single entry in the
 * row or column spec.
 */
struct nsFramesetSpec {
  nsFramesetUnit mUnit;
  nscoord        mValue;
};

/**
 * The maximum number of entries allowed in the frame set element row
 * or column spec.
 */
#define NS_MAX_FRAMESET_SPEC_COUNT 16000

//----------------------------------------------------------------------

namespace mozilla {
namespace dom {

class OnBeforeUnloadEventHandlerNonNull;

class HTMLFrameSetElement MOZ_FINAL : public nsGenericHTMLElement,
                                      public nsIDOMHTMLFrameSetElement
{
public:
  explicit HTMLFrameSetElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo),
      mNumRows(0),
      mNumCols(0),
      mCurrentRowColHint(NS_STYLE_HINT_REFLOW)
  {
    SetHasWeirdParserInsertionMode();
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLFrameSetElement, frameset)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLFrameSetElement
  NS_DECL_NSIDOMHTMLFRAMESETELEMENT

  void GetCols(nsString& aCols)
  {
    GetHTMLAttr(nsGkAtoms::cols, aCols);
  }
  void SetCols(const nsAString& aCols, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::cols, aCols, aError);
  }
  void GetRows(nsString& aRows)
  {
    GetHTMLAttr(nsGkAtoms::rows, aRows);
  }
  void SetRows(const nsAString& aRows, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::rows, aRows, aError);
  }

  virtual bool IsEventAttributeName(nsIAtom *aName) MOZ_OVERRIDE;

  // Event listener stuff; we need to declare only the ones we need to
  // forward to window that don't come from nsIDOMHTMLFrameSetElement.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the superclass */
#define WINDOW_EVENT_HELPER(name_, type_)                               \
  type_* GetOn##name_();                                                \
  void SetOn##name_(type_* handler);
#define WINDOW_EVENT(name_, id_, type_, struct_)                        \
  WINDOW_EVENT_HELPER(name_, EventHandlerNonNull)
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                  \
  WINDOW_EVENT_HELPER(name_, OnBeforeUnloadEventHandlerNonNull)
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef BEFOREUNLOAD_EVENT
#undef WINDOW_EVENT
#undef WINDOW_EVENT_HELPER
#undef EVENT

  // These override the SetAttr methods in nsGenericHTMLElement (need
  // both here to silence compiler warnings).
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;

   /**
    * GetRowSpec is used to get the "rows" spec.
    * @param out int32_t aNumValues The number of row sizes specified.
    * @param out nsFramesetSpec* aSpecs The array of size specifications.
             This is _not_ owned by the caller, but by the nsFrameSetElement
             implementation.  DO NOT DELETE IT.
    */
  nsresult GetRowSpec(int32_t *aNumValues, const nsFramesetSpec** aSpecs);
   /**
    * GetColSpec is used to get the "cols" spec
    * @param out int32_t aNumValues The number of row sizes specified.
    * @param out nsFramesetSpec* aSpecs The array of size specifications.
             This is _not_ owned by the caller, but by the nsFrameSetElement
             implementation.  DO NOT DELETE IT.
    */
  nsresult GetColSpec(int32_t *aNumValues, const nsFramesetSpec** aSpecs);


  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

protected:
  virtual ~HTMLFrameSetElement();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

private:
  nsresult ParseRowCol(const nsAString& aValue,
                       int32_t&         aNumSpecs,
                       nsFramesetSpec** aSpecs);

  /**
   * The number of size specs in our "rows" attr
   */
  int32_t          mNumRows;
  /**
   * The number of size specs in our "cols" attr
   */
  int32_t          mNumCols;
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

} // namespace dom
} // namespace mozilla

#endif // HTMLFrameSetElement_h
