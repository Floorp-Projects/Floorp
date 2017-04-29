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
    , mNetworkCreated(aFromParser == mozilla::dom::FROM_PARSER_NETWORK)
    , mIsPrerendered(false)
    , mBrowserFrameListenersRegistered(false)
    , mFrameLoaderCreationDisallowed(false)
    , mReallyIsBrowser(false)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIFRAMELOADEROWNER
  NS_DECL_NSIDOMMOZBROWSERFRAME
  NS_DECL_NSIMOZBROWSERFRAME

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) override;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify) override;
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

protected:
  virtual ~nsGenericHTMLFrameElement();

  // This doesn't really ensure a frame loader in all cases, only when
  // it makes sense.
  void EnsureFrameLoader();
  nsresult LoadSrc();
  nsIDocument* GetContentDocument(nsIPrincipal& aSubjectPrincipal);
  nsresult GetContentDocument(nsIDOMDocument** aContentDocument);
  already_AddRefed<nsPIDOMWindowOuter> GetContentWindow();

  RefPtr<nsFrameLoader> mFrameLoader;
  nsCOMPtr<nsPIDOMWindowOuter> mOpenerWindow;

  /**
   * True when the element is created by the parser using the
   * NS_FROM_PARSER_NETWORK flag.
   * If the element is modified, it may lose the flag.
   */
  bool mNetworkCreated;

  bool mIsPrerendered;
  bool mBrowserFrameListenersRegistered;
  bool mFrameLoaderCreationDisallowed;
  bool mReallyIsBrowser;

  // This flag is only used by <iframe>. See HTMLIFrameElement::
  // FullscreenFlag() for details. It is placed here so that we
  // do not bloat any struct.
  bool mFullscreenFlag = false;

private:
  void GetManifestURL(nsAString& aOut);
};

#endif // nsGenericHTMLFrameElement_h
