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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

#ifndef NS_SVGMPATHELEMENT_H_
#define NS_SVGMPATHELEMENT_H_

#include "nsIDOMSVGMpathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGPathElement.h"
#include "nsSVGString.h"
#include "nsReferencedElement.h"


typedef nsSVGElement nsSVGMpathElementBase;

class nsSVGMpathElement : public nsSVGMpathElementBase,
                          public nsIDOMSVGMpathElement,
                          public nsIDOMSVGURIReference,
                          public nsStubMutationObserver
{
protected:
  friend nsresult NS_NewSVGMpathElement(nsIContent **aResult,
                                        already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGMpathElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  ~nsSVGMpathElement();


public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGMPATHELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSVGMpathElement,
                                           nsSVGMpathElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  // Forward interface implementations to base class
  NS_FORWARD_NSIDOMNODE(nsSVGMpathElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGMpathElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGMpathElementBase::)

  // nsIContent interface
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  virtual nsresult UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                             bool aNotify);
  // nsGenericElement specializations
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  // Public helper method: If our xlink:href attribute links to a <path>
  // element, this method returns a pointer to that element. Otherwise,
  // this returns nsnull.
  nsSVGPathElement* GetReferencedPath();

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  class PathReference : public nsReferencedElement {
  public:
    PathReference(nsSVGMpathElement* aMpathElement) :
      mMpathElement(aMpathElement) {}
  protected:
    // We need to be notified when target changes, in order to request a sample
    // (which will clear animation effects that used the old target-path
    // and recompute the animation effects using the new target-path).
    virtual void ElementChanged(Element* aFrom, Element* aTo) {
      nsReferencedElement::ElementChanged(aFrom, aTo);
      if (aFrom) {
        aFrom->RemoveMutationObserver(mMpathElement);
      }
      if (aTo) {
        aTo->AddMutationObserver(mMpathElement);
      }
      mMpathElement->NotifyParentOfMpathChange(mMpathElement->GetParent());
    }

    // We need to override IsPersistent to get persistent tracking (beyond the
    // first time the target changes)
    virtual bool IsPersistent() { return true; }
  private:
    nsSVGMpathElement* const mMpathElement;
  };

  virtual StringAttributesInfo GetStringInfo();

  void UpdateHrefTarget(nsIContent* aParent, const nsAString& aHrefStr);
  void UnlinkHrefTarget(bool aNotifyParent);
  void NotifyParentOfMpathChange(nsIContent* aParent);

  enum { HREF };
  nsSVGString        mStringAttributes[1];
  static StringInfo  sStringInfo[1];
  PathReference      mHrefTarget;
};

#endif // NS_SVGMPATHELEMENT_H_
