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
#include "nsFrameLoaderOwner.h"
#include "nsGenericHTMLElement.h"
#include "nsIMozBrowserFrame.h"

namespace mozilla {
namespace dom {
class BrowserParent;
template <typename>
struct Nullable;
class WindowProxyHolder;
class XULFrameElement;
}  // namespace dom
}  // namespace mozilla

#define NS_GENERICHTMLFRAMEELEMENT_IID               \
  {                                                  \
    0x8190db72, 0xdab0, 0x4d72, {                    \
      0x94, 0x26, 0x87, 0x5f, 0x5a, 0x8a, 0x2a, 0xe5 \
    }                                                \
  }

/**
 * A helper class for frame elements
 */
class nsGenericHTMLFrameElement : public nsGenericHTMLElement,
                                  public nsFrameLoaderOwner,
                                  public mozilla::nsBrowserElement,
                                  public nsIMozBrowserFrame {
 public:
  nsGenericHTMLFrameElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      mozilla::dom::FromParser aFromParser)
      : nsGenericHTMLElement(std::move(aNodeInfo)),
        nsBrowserElement(),
        mSrcLoadHappened(false),
        mNetworkCreated(aFromParser == mozilla::dom::FROM_PARSER_NETWORK),
        mBrowserFrameListenersRegistered(false),
        mFrameLoaderCreationDisallowed(false),
        mReallyIsBrowser(false) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMMOZBROWSERFRAME
  NS_DECL_NSIMOZBROWSERFRAME

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_GENERICHTMLFRAMEELEMENT_IID)

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex) override;
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;
  virtual void DestroyContent() override;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  virtual int32_t TabIndexDefault() override;

  virtual nsIMozBrowserFrame* GetAsMozBrowserFrame() override { return this; }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsGenericHTMLFrameElement,
                                           nsGenericHTMLElement)

  void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& aError);

  void SwapFrameLoaders(mozilla::dom::XULFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& aError);

  void SwapFrameLoaders(nsFrameLoaderOwner* aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);

  /**
   * Normally, a frame tries to create its frame loader when its src is
   * modified, or its contentWindow is accessed.
   *
   * disallowCreateFrameLoader prevents the frame element from creating its
   * frame loader (in the same way that not being inside a document prevents the
   * creation of a frame loader).  allowCreateFrameLoader lifts this
   * restriction.
   *
   * These methods are not re-entrant -- it is an error to call
   * disallowCreateFrameLoader twice without first calling allowFrameLoader.
   *
   * It's also an error to call either method if we already have a frame loader.
   */
  void DisallowCreateFrameLoader();
  void AllowCreateFrameLoader();

  /**
   * Create a remote (i.e., out-of-process) frame loader attached to the given
   * remote tab.
   *
   * It is an error to call this method if we already have a frame loader.
   */
  void CreateRemoteFrameLoader(mozilla::dom::BrowserParent* aBrowserParent);

  /**
   * Helper method to map a HTML 'scrolling' attribute value (which can be null)
   * to a ScrollbarPreference value value.  scrolling="no" (and its synonyms)
   * map to Never, and anything else to Auto.
   */
  static mozilla::ScrollbarPreference MapScrollingAttribute(const nsAttrValue*);

  nsIPrincipal* GetSrcTriggeringPrincipal() const {
    return mSrcTriggeringPrincipal;
  }

  // Needed for nsBrowserElement
  already_AddRefed<nsFrameLoader> GetFrameLoader() override {
    return nsFrameLoaderOwner::GetFrameLoader();
  }

 protected:
  virtual ~nsGenericHTMLFrameElement();

  // This doesn't really ensure a frame loader in all cases, only when
  // it makes sense.
  void EnsureFrameLoader();
  void LoadSrc();
  Document* GetContentDocument(nsIPrincipal& aSubjectPrincipal);
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> GetContentWindow();

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual nsresult OnAttrSetButNotChanged(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify) override;

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

  mozilla::dom::BrowsingContext* GetContentWindowInternal();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsGenericHTMLFrameElement,
                              NS_GENERICHTMLFRAMEELEMENT_IID)

#endif  // nsGenericHTMLFrameElement_h
