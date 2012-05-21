/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLFrameSetElement_h
#define nsHTMLFrameSetElement_h

#include "nsISupports.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"

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

class nsHTMLFrameSetElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLFrameSetElement
{
public:
  nsHTMLFrameSetElement(already_AddRefed<nsINodeInfo> aNodeInfo);
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

  // Event listener stuff; we need to declare only the ones we need to
  // forward to window that don't come from nsIDOMHTMLFrameSetElement.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the superclass */
#define FORWARDED_EVENT(name_, id_, type_, struct_)                     \
    NS_IMETHOD GetOn##name_(JSContext *cx, jsval *vp);            \
    NS_IMETHOD SetOn##name_(JSContext *cx, const jsval &v);
#include "nsEventNameList.h"
#undef FORWARDED_EVENT
#undef EVENT

  // These override the SetAttr methods in nsGenericHTMLElement (need
  // both here to silence compiler warnings).
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);

   /**
    * GetRowSpec is used to get the "rows" spec.
    * @param out PRInt32 aNumValues The number of row sizes specified.
    * @param out nsFramesetSpec* aSpecs The array of size specifications.
             This is _not_ owned by the caller, but by the nsFrameSetElement
             implementation.  DO NOT DELETE IT.
    */
  nsresult GetRowSpec(PRInt32 *aNumValues, const nsFramesetSpec** aSpecs);
   /**
    * GetColSpec is used to get the "cols" spec
    * @param out PRInt32 aNumValues The number of row sizes specified.
    * @param out nsFramesetSpec* aSpecs The array of size specifications.
             This is _not_ owned by the caller, but by the nsFrameSetElement
             implementation.  DO NOT DELETE IT.
    */
  nsresult GetColSpec(PRInt32 *aNumValues, const nsFramesetSpec** aSpecs);


  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }
  static nsHTMLFrameSetElement* FromContent(nsIContent *aContent)
  {
    if (aContent->IsHTML(nsGkAtoms::frameset))
      return static_cast<nsHTMLFrameSetElement*>(aContent);
    return nsnull;
  }

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

#endif // nsHTMLFrameSetElement_h
