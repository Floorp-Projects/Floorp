/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGUSEELEMENT_H_
#define DOM_SVG_SVGUSEELEMENT_H_

#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/IDTracker.h"
#include "mozilla/dom/SVGGraphicsElement.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsStubMutationObserver.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "nsTArray.h"

class nsIContent;

nsresult NS_NewSVGSVGElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser);
nsresult NS_NewSVGUseElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class Encoding;
class SVGUseFrame;
struct URLExtraData;

namespace dom {

using SVGUseElementBase = SVGGraphicsElement;

class SVGUseElement final : public SVGUseElementBase,
                            public nsStubMutationObserver {
  friend class mozilla::SVGUseFrame;

 protected:
  friend nsresult(::NS_NewSVGUseElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGUseElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual ~SVGUseElement();
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  NS_IMPL_FROMNODE_WITH_TAG(SVGUseElement, kNameSpaceID_SVG, use)

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(UnbindContext&) override;

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGUseElement, SVGUseElementBase)

  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  // SVGElement specializations:
  gfxMatrix PrependLocalTransformsTo(
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const override;
  bool HasValidDimensions() const override;

  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  static nsCSSPropertyID GetCSSPropertyIdForAttrEnum(uint8_t aAttrEnum);

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();

  nsIURI* GetSourceDocURI();
  const Encoding* GetSourceDocCharacterSet();
  URLExtraData* GetContentURLData() const { return mContentURLData; }

  // Updates the internal shadow tree to be an up-to-date clone of the
  // referenced element.
  void UpdateShadowTree();

  // Shared code between AfterSetAttr and SVGUseFrame::AttributeChanged.
  //
  // This is needed because SMIL doesn't go through AfterSetAttr unfortunately.
  void ProcessAttributeChange(int32_t aNamespaceID, nsAtom* aAttribute);

  void AfterSetAttr(int32_t aNamespaceID, nsAtom* aAttribute,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) final;

 protected:
  // Information from walking our ancestors and a given target.
  enum class ScanResult {
    // Nothing that should stop us from rendering the shadow tree.
    Ok,
    // We're never going to be displayed, so no point in updating the shadow
    // tree.
    //
    // However if we're referenced from another tree that tree may need to be
    // rendered.
    Invisible,
    // We're a cyclic reference to either an ancestor or another shadow tree. We
    // shouldn't render this <use> element.
    CyclicReference,
    // We're too deep in our clone chain, we shouldn't be rendered.
    TooDeep,
  };
  ScanResult ScanAncestors(const Element& aTarget) const;
  ScanResult ScanAncestorsInternal(const Element& aTarget,
                                   uint32_t& aCount) const;

  /**
   * Helper that provides a reference to the element with the ID that is
   * referenced by the 'use' element's 'href' attribute, and that will update
   * the 'use' element if the element that that ID identifies changes to a
   * different element (or none).
   */
  class ElementTracker final : public IDTracker {
   public:
    explicit ElementTracker(SVGUseElement* aOwningUseElement)
        : mOwningUseElement(aOwningUseElement) {}

   private:
    void ElementChanged(Element* aFrom, Element* aTo) override {
      IDTracker::ElementChanged(aFrom, aTo);
      if (aFrom) {
        aFrom->RemoveMutationObserver(mOwningUseElement);
      }
      mOwningUseElement->TriggerReclone();
    }

    SVGUseElement* mOwningUseElement;
  };

  void DidAnimateAttribute(int32_t aNameSpaceID, nsAtom* aAttribute) override;
  SVGUseFrame* GetFrame() const;

  LengthAttributesInfo GetLengthInfo() override;
  StringAttributesInfo GetStringInfo() override;

  /**
   * Returns true if our width and height should be used, or false if they
   * should be ignored. As per the spec, this depends on the type of the
   * element that we're referencing.
   */
  bool OurWidthAndHeightAreUsed() const;
  void SyncWidthOrHeight(nsAtom* aName);
  void LookupHref();
  void TriggerReclone();
  void UnlinkSource();

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  RefPtr<SVGUseElement> mOriginal;  // if we've been cloned, our "real" copy
  ElementTracker mReferencedElementTracker;
  RefPtr<URLExtraData> mContentURLData;  // URL data for its anonymous content
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGUSEELEMENT_H_
