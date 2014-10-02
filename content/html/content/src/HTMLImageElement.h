/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLImageElement_h
#define mozilla_dom_HTMLImageElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsImageLoadingContent.h"
#include "nsIDOMHTMLImageElement.h"
#include "imgRequestProxy.h"
#include "Units.h"

// Only needed for IsPictureEnabled()
#include "mozilla/dom/HTMLPictureElement.h"

namespace mozilla {
class EventChainPreVisitor;
namespace dom {

class ResponsiveImageSelector;
class HTMLImageElement MOZ_FINAL : public nsGenericHTMLElement,
                                   public nsImageLoadingContent,
                                   public nsIDOMHTMLImageElement
{
  friend class HTMLSourceElement;
  friend class HTMLPictureElement;
  friend class ImageLoadTask;
public:
  explicit HTMLImageElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  static already_AddRefed<HTMLImageElement>
    Image(const GlobalObject& aGlobal,
          const Optional<uint32_t>& aWidth,
          const Optional<uint32_t>& aHeight,
          ErrorResult& aError);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual bool Draggable() const MOZ_OVERRIDE;

  // nsIDOMHTMLImageElement
  NS_DECL_NSIDOMHTMLIMAGEELEMENT

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLImageElement, img)

  // override from nsImageLoadingContent
  CORSMode GetCORSMode();

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;

  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) MOZ_OVERRIDE;

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;

  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  nsresult CopyInnerTo(Element* aDest);

  void MaybeLoadImage();

  static bool IsSrcsetEnabled();

  bool IsMap()
  {
    return GetBoolAttr(nsGkAtoms::ismap);
  }
  void SetIsMap(bool aIsMap, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::ismap, aIsMap, aError);
  }
  uint32_t Width()
  {
    return GetWidthHeightForImage(mCurrentRequest).width;
  }
  void SetWidth(uint32_t aWidth, ErrorResult& aError)
  {
    SetUnsignedIntAttr(nsGkAtoms::width, aWidth, aError);
  }
  uint32_t Height()
  {
    return GetWidthHeightForImage(mCurrentRequest).height;
  }
  void SetHeight(uint32_t aHeight, ErrorResult& aError)
  {
    SetUnsignedIntAttr(nsGkAtoms::height, aHeight, aError);
  }
  uint32_t NaturalWidth();
  uint32_t NaturalHeight();
  bool Complete();
  uint32_t Hspace()
  {
    return GetUnsignedIntAttr(nsGkAtoms::hspace, 0);
  }
  void SetHspace(uint32_t aHspace, ErrorResult& aError)
  {
    SetUnsignedIntAttr(nsGkAtoms::hspace, aHspace, aError);
  }
  uint32_t Vspace()
  {
    return GetUnsignedIntAttr(nsGkAtoms::vspace, 0);
  }
  void SetVspace(uint32_t aVspace, ErrorResult& aError)
  {
    SetUnsignedIntAttr(nsGkAtoms::vspace, aVspace, aError);
  }

  // The XPCOM versions of the following getters work for Web IDL bindings as well
  void SetAlt(const nsAString& aAlt, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::alt, aAlt, aError);
  }
  void SetSrc(const nsAString& aSrc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aError);
  }
  void SetSrcset(const nsAString& aSrcset, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::srcset, aSrcset, aError);
  }
  void GetCrossOrigin(nsAString& aResult)
  {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aResult);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError)
  {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }
  void SetUseMap(const nsAString& aUseMap, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::usemap, aUseMap, aError);
  }
  void SetName(const nsAString& aName, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void SetLongDesc(const nsAString& aLongDesc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::longdesc, aLongDesc, aError);
  }
  void SetSizes(const nsAString& aSizes, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::sizes, aSizes, aError);
  }
  void SetBorder(const nsAString& aBorder, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::border, aBorder, aError);
  }

  int32_t X();
  int32_t Y();
  // Uses XPCOM GetLowsrc.
  void SetLowsrc(const nsAString& aLowsrc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::lowsrc, aLowsrc, aError);
  }

#ifdef DEBUG
  nsIDOMHTMLFormElement* GetForm() const;
#endif
  void SetForm(nsIDOMHTMLFormElement* aForm);
  void ClearForm(bool aRemoveFromForm);

  virtual void DestroyContent() MOZ_OVERRIDE;

protected:
  virtual ~HTMLImageElement();

  // Queues a task to run LoadSelectedImage pending stable state
  void QueueImageLoadTask();

  // Resolve and load the current mResponsiveSelector (responsive mode) or src
  // attr image.
  nsresult LoadSelectedImage(bool aForce, bool aNotify);

  // Update/create/destroy mResponsiveSelector
  void PictureSourceSrcsetChanged(nsIContent *aSourceNode,
                                  const nsAString& aNewValue, bool aNotify);
  void PictureSourceSizesChanged(nsIContent *aSourceNode,
                                 const nsAString& aNewValue, bool aNotify);
  void PictureSourceMediaChanged(nsIContent *aSourceNode,
                                 const nsAString& aNewValue, bool aNotify);

  void PictureSourceAdded(nsIContent *aSourceNode);
  // This should be called prior to the unbind, such that nextsibling works
  void PictureSourceRemoved(nsIContent *aSourceNode);

  // Re-evaluates all source nodes (picture <source>,<img>) and finds
  // the best source set for mResponsiveSelector. If a better source
  // is found, creates a new selector and feeds the source to it. If
  // the current ResponsiveSelector is not changed, runs
  // SelectImage(true) to re-evaluate its candidates.
  //
  // Because keeping the existing selector is the common case (and we
  // often do no-op reselections), this does not re-parse values for
  // the existing mResponsiveSelector, meaning you need to update its
  // parameters as appropriate before calling (or null it out to force
  // recreation)
  void UpdateResponsiveSource();

  // Given a <source> node that is a previous sibling *or* ourselves, try to
  // create a ResponsiveSelector.

  // If the node's srcset/sizes make for an invalid selector, returns
  // false. This does not guarantee the resulting selector matches an image,
  // only that it is valid.
  bool TryCreateResponsiveSelector(nsIContent *aSourceNode,
                                   const nsAString *aSrcset = nullptr,
                                   const nsAString *aSizes = nullptr);

  CSSIntPoint GetXY();
  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
  void UpdateFormOwner();

  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) MOZ_OVERRIDE;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  // This is a weak reference that this element and the HTMLFormElement
  // cooperate in maintaining.
  HTMLFormElement* mForm;

  // Created when we're tracking responsive image state
  nsRefPtr<ResponsiveImageSelector> mResponsiveSelector;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);

  nsCOMPtr<nsIRunnable> mPendingImageLoadTask;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLImageElement_h */
