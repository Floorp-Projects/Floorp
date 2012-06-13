/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "nsIFrameLoader.h"
#include "nsIMozBrowserFrame.h"
#include "nsIDOMEventListener.h"

#include "nsFrameLoader.h"

/**
 * A helper class for frame elements
 */
class nsGenericHTMLFrameElement : public nsGenericHTMLElement,
                                  public nsIFrameLoaderOwner,
                                  public nsIMozBrowserFrame
{
public:
  nsGenericHTMLFrameElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                            mozilla::dom::FromParser aFromParser)
    : nsGenericHTMLElement(aNodeInfo)
    , mNetworkCreated(aFromParser == mozilla::dom::FROM_PARSER_NETWORK)
    , mBrowserFrameListenersRegistered(false)
    , mFrameLoaderCreationDisallowed(false)
  {
  }

  virtual ~nsGenericHTMLFrameElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_DECL_NSIFRAMELOADEROWNER
  NS_DECL_NSIDOMMOZBROWSERFRAME
  NS_DECL_NSIMOZBROWSERFRAME

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual void DestroyContent();

  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  // nsIDOMHTMLElement
  NS_IMETHOD GetTabIndex(PRInt32 *aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsGenericHTMLFrameElement,
                                                     nsGenericHTMLElement)

protected:
  /**
   * Listens to titlechanged events from the document inside the iframe and
   * forwards them along to the iframe so it can fire a mozbrowsertitlechange
   * event if appropriate.
   */
  class TitleChangedListener MOZ_FINAL : public nsIDOMEventListener
  {
  public:
    TitleChangedListener(nsGenericHTMLFrameElement *aElement,
                         nsIDOMEventTarget *aChromeHandler);

    /* Unregister this listener. */
    void Unregister();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER

  private:
    nsWeakPtr mElement; /* nsGenericHTMLFrameElement */
    nsWeakPtr mChromeHandler; /* nsIDOMEventTarget */
  };

  // This doesn't really ensure a frame loade in all cases, only when
  // it makes sense.
  nsresult EnsureFrameLoader();
  nsresult LoadSrc();
  nsresult GetContentDocument(nsIDOMDocument** aContentDocument);
  nsresult GetContentWindow(nsIDOMWindow** aContentWindow);

  nsRefPtr<nsFrameLoader> mFrameLoader;

  // True when the element is created by the parser
  // using NS_FROM_PARSER_NETWORK flag.
  // If the element is modified, it may lose the flag.
  bool                    mNetworkCreated;

  bool                    mBrowserFrameListenersRegistered;
  bool                    mFrameLoaderCreationDisallowed;
};
