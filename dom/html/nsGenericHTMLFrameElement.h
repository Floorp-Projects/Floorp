/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGenericHTMLFrameElement_h
#define nsGenericHTMLFrameElement_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/nsBrowserElement.h"

#include "nsFrameLoader.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMEventListener.h"
#include "nsIFrameLoader.h"
#include "nsIMozBrowserFrame.h"

class nsXULElement;

#define NS_GENERICHTMLFRAMEELEMENT_IID \
{ 0x8190db72, 0xdab0, 0x4d72, \
  { 0x94, 0x26, 0x87, 0x5f, 0x5a, 0x8a, 0x2a, 0xe5 } }

/**
 * A helper class for frame elements
 */
class nsGenericHTMLFrameElement : public nsGenericHTMLElement,
                                  public nsIFrameLoaderOwner,
                                  public mozilla::nsBrowserElement,
                                  public nsIMozBrowserFrame
{
public:
  nsGenericHTMLFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                            mozilla::dom::FromParser aFromParser)
    : nsGenericHTMLElement(aNodeInfo)
    , nsBrowserElement()
    , mSrcLoadHappened(false)
    , mNetworkCreated(aFromParser == mozilla::dom::FROM_PARSER_NETWORK)
    , mBrowserFrameListenersRegistered(false)
    , mFrameLoaderCreationDisallowed(false)
    , mReallyIsBrowser(false)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIFRAMELOADEROWNER
  NS_DECL_NSIDOMMOZBROWSERFRAME
  NS_DECL_NSIMOZBROWSERFRAME

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_GENERICHTMLFRAMEELEMENT_IID)

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) override;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual void DestroyContent() override;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest, bool aPreallocateChildren);

  virtual int32_t TabIndexDefault() override;

  virtual nsIMozBrowserFrame* GetAsMozBrowserFrame() override { return this; }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsGenericHTMLFrameElement,
                                           nsGenericHTMLElement)

  void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& aError);

  void SwapFrameLoaders(nsXULElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& aError);

  void SwapFrameLoaders(nsIFrameLoaderOwner* aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);

  void PresetOpenerWindow(mozIDOMWindowProxy* aOpenerWindow,
                          mozilla::ErrorResult& aRv);

  static void InitStatics();
  static bool BrowserFramesEnabled();

  /**
   * Helper method to map a HTML 'scrolling' attribute value to a nsIScrollable
   * enum value.  scrolling="no" (and its synonyms) maps to
   * nsIScrollable::Scrollbar_Never, and anything else (including nullptr) maps
   * to nsIScrollable::Scrollbar_Auto.
   * @param aValue the attribute value to map or nullptr
   * @return nsIScrollable::Scrollbar_Never or nsIScrollable::Scrollbar_Auto
   */
  static int32_t MapScrollingAttribute(const nsAttrValue* aValue);

  nsIPrincipal* GetSrcTriggeringPrincipal() const
  {
    return mSrcTriggeringPrincipal;
  }

protected:
  virtual ~nsGenericHTMLFrameElement();

  // This doesn't really ensure a frame loader in all cases, only when
  // it makes sense.
  void EnsureFrameLoader();
  nsresult LoadSrc();
  nsIDocument* GetContentDocument(nsIPrincipal& aSubjectPrincipal);
  nsresult GetContentDocument(nsIDOMDocument** aContentDocument);
  already_AddRefed<nsPIDOMWindowOuter> GetContentWindow();

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual nsresult OnAttrSetButNotChanged(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify) override;

  RefPtr<nsFrameLoader> mFrameLoader;
  nsCOMPtr<nsPIDOMWindowOuter> mOpenerWindow;

  nsCOMPtr<nsIPrincipal> mSrcTriggeringPrincipal;

  /**
   * True if we have already loaded the frame's original src
   */
  bool mSrcLoadHappened;

  /**
   * True when the element is created by the parser using the
   * NS_FROM_PARSER_NETWORK flag.
   * If the element is modified, it may lose the flag.
   */
  bool mNetworkCreated;

  bool mBrowserFrameListenersRegistered;
  bool mFrameLoaderCreationDisallowed;
  bool mReallyIsBrowser;

  // This flag is only used by <iframe>. See HTMLIFrameElement::
  // FullscreenFlag() for details. It is placed here so that we
  // do not bloat any struct.
  bool mFullscreenFlag = false;

private:
  void GetManifestURL(nsAString& aOut);

  /**
   * This function is called by AfterSetAttr and OnAttrSetButNotChanged.
   * It will be called whether the value is being set or unset.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value being set or null if the value is being unset
   * @param aNotify Whether we plan to notify document observers.
   */
  void AfterMaybeChangeAttr(int32_t aNamespaceID, nsAtom* aName,
                            const nsAttrValueOrString* aValue,
                            nsIPrincipal* aMaybeScriptedPrincipal,
                            bool aNotify);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsGenericHTMLFrameElement,
                              NS_GENERICHTMLFRAMEELEMENT_IID)

#endif // nsGenericHTMLFrameElement_h
