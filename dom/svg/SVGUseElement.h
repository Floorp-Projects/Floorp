/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGUseElement_h
#define mozilla_dom_SVGUseElement_h

#include "mozilla/dom/FromParser.h"
#include "nsReferencedElement.h"
#include "nsStubMutationObserver.h"
#include "mozilla/dom/SVGGraphicsElement.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "nsTArray.h"

class nsIContent;
class nsSVGUseFrame;

nsresult
NS_NewSVGSVGElement(nsIContent **aResult,
                    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                    mozilla::dom::FromParser aFromParser);
nsresult NS_NewSVGUseElement(nsIContent **aResult,
                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
struct URLExtraData;

namespace dom {

typedef SVGGraphicsElement SVGUseElementBase;

class SVGUseElement final : public SVGUseElementBase,
                            public nsStubMutationObserver
{
  friend class ::nsSVGUseFrame;
protected:
  friend nsresult (::NS_NewSVGUseElement(nsIContent **aResult,
                                         already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGUseElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~SVGUseElement();
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGUseElement, SVGUseElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  // for nsSVGUseFrame's nsIAnonymousContentCreator implementation.
  nsIContent* CreateAnonymousContent();
  nsIContent* GetAnonymousContent() const { return mClone; }
  void DestroyAnonymousContent();

  // nsSVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(
    const gfxMatrix &aMatrix,
    SVGTransformTypes aWhich = eAllTransforms) const override;
  virtual bool HasValidDimensions() const override;

  // nsIContent interface
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedString> Href();
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();

  nsIURI* GetSourceDocURI();
  URLExtraData* GetContentURLData() const { return mContentURLData; }

protected:
  class SourceReference : public nsReferencedElement {
  public:
    explicit SourceReference(SVGUseElement* aContainer) : mContainer(aContainer) {}
  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) override {
      nsReferencedElement::ElementChanged(aFrom, aTo);
      if (aFrom) {
        aFrom->RemoveMutationObserver(mContainer);
      }
      mContainer->TriggerReclone();
    }
  private:
    SVGUseElement* mContainer;
  };

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  /**
   * Returns true if our width and height should be used, or false if they
   * should be ignored. As per the spec, this depends on the type of the
   * element that we're referencing.
   */
  bool OurWidthAndHeightAreUsed() const;
  void SyncWidthOrHeight(nsIAtom *aName);
  void LookupHref();
  void TriggerReclone();
  void UnlinkSource();

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { HREF, XLINK_HREF };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  nsCOMPtr<nsIContent> mOriginal; // if we've been cloned, our "real" copy
  nsCOMPtr<nsIContent> mClone;    // cloned tree
  SourceReference      mSource;   // observed element
  RefPtr<URLExtraData> mContentURLData; // URL data for its anonymous content
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGUseElement_h
