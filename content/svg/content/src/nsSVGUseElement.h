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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef __NS_SVGUSEELEMENT_H__
#define __NS_SVGUSEELEMENT_H__

#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGUseElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "nsTArray.h"
#include "nsReferencedElement.h"

class nsIContent;
class nsINodeInfo;

#define NS_SVG_USE_ELEMENT_IMPL_CID \
{ 0x55fb86fe, 0xd81f, 0x4ae4, \
  { 0x80, 0x3f, 0xeb, 0x90, 0xfe, 0xe0, 0x7a, 0xe9 } }

nsresult
NS_NewSVGSVGElement(nsIContent **aResult,
                    already_AddRefed<nsINodeInfo> aNodeInfo,
                    PRUint32 aFromParser);

typedef nsSVGGraphicElement nsSVGUseElementBase;

class nsSVGUseElement : public nsSVGUseElementBase,
                        public nsIDOMSVGURIReference,
                        public nsIDOMSVGUseElement,
                        public nsStubMutationObserver
{
  friend class nsSVGUseFrame;
protected:
  friend nsresult NS_NewSVGUseElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGUseElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsSVGUseElement();
  
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_USE_ELEMENT_IMPL_CID)

  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSVGUseElement, nsSVGUseElementBase)
  NS_DECL_NSIDOMSVGUSEELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGUseElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGUseElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGUseElementBase::)

  // for nsSVGUseFrame's nsIAnonymousContentCreator implementation.
  nsIContent* CreateAnonymousContent();
  nsIContent* GetAnonymousContent() const { return mClone; }
  void DestroyAnonymousContent();

  // nsSVGElement specializations:
  virtual gfxMatrix PrependLocalTransformTo(const gfxMatrix &aMatrix);
  virtual void DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeString(PRUint8 aAttrEnum);
  virtual void DidAnimateString(PRUint8 aAttrEnum);

  // nsIContent interface
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  class SourceReference : public nsReferencedElement {
  public:
    SourceReference(nsSVGUseElement* aContainer) : mContainer(aContainer) {}
  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) {
      nsReferencedElement::ElementChanged(aFrom, aTo);
      if (aFrom) {
        aFrom->RemoveMutationObserver(mContainer);
      }
      mContainer->TriggerReclone();
    }
  private:
    nsSVGUseElement* mContainer;
  };

  virtual LengthAttributesInfo GetLengthInfo();
  virtual StringAttributesInfo GetStringInfo();

  void SyncWidthHeight(PRUint8 aAttrEnum);
  void LookupHref();
  void TriggerReclone();
  void UnlinkSource();

  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  nsCOMPtr<nsIContent> mOriginal; // if we've been cloned, our "real" copy
  nsCOMPtr<nsIContent> mClone;    // cloned tree
  SourceReference      mSource;   // observed element
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGUseElement, NS_SVG_USE_ELEMENT_IMPL_CID)

#endif
