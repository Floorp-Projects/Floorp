/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGenericHTMLFrameElement_h
#define nsGenericHTMLFrameElement_h

#include "mozilla/Attributes.h"
#include "nsElementFrameLoaderOwner.h"
#include "nsGenericHTMLElement.h"
#include "nsIFrameLoader.h"
#include "nsIMozBrowserFrame.h"
#include "nsIDOMEventListener.h"
#include "mozilla/ErrorResult.h"

#include "nsFrameLoader.h"

class nsXULElement;

/**
 * A helper class for frame elements
 */
class nsGenericHTMLFrameElement : public nsGenericHTMLElement,
                                  public nsElementFrameLoaderOwner,
                                  public nsIMozBrowserFrame
{
public:
  nsGenericHTMLFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                            mozilla::dom::FromParser aFromParser)
    : nsGenericHTMLElement(aNodeInfo)
    , nsElementFrameLoaderOwner(aFromParser)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMMOZBROWSERFRAME
  NS_DECL_NSIMOZBROWSERFRAME

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) MOZ_OVERRIDE;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify) MOZ_OVERRIDE;
  virtual void DestroyContent() MOZ_OVERRIDE;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsGenericHTMLFrameElement,
                                                     nsGenericHTMLElement)

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
  virtual ~nsGenericHTMLFrameElement() {}

  virtual mozilla::dom::Element* ThisFrameElement() MOZ_OVERRIDE
  {
    return this;
  }
};

#endif // nsGenericHTMLFrameElement_h
