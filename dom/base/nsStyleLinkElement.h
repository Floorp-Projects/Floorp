/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class which implements nsIStyleSheetLinkingElement and can
 * be subclassed by various content nodes that want to load
 * stylesheets (<style>, <link>, processing instructions, etc).
 */

#ifndef nsStyleLinkElement_h___
#define nsStyleLinkElement_h___

#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "nsCOMPtr.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsTArray.h"

class nsIDocument;
class nsIURI;

namespace mozilla {
class CSSStyleSheet;
namespace dom {
class ShadowRoot;
} // namespace dom
} // namespace mozilla

class nsStyleLinkElement : public nsIStyleSheetLinkingElement
{
public:
  nsStyleLinkElement();
  virtual ~nsStyleLinkElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override = 0;

  mozilla::CSSStyleSheet* GetSheet() const
  {
    // XXXheycam Return nullptr for ServoStyleSheets until we have a way of
    // exposing them to script.
    NS_ASSERTION(!mStyleSheet || mStyleSheet->IsGecko(),
                 "stylo: ServoStyleSheets can't be exposed to script yet");
    return mStyleSheet && mStyleSheet->IsGecko() ? mStyleSheet->AsGecko() :
                                                   nullptr;
  }

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(mozilla::StyleSheetHandle aStyleSheet) override;
  NS_IMETHOD_(mozilla::StyleSheetHandle) GetStyleSheet() override;
  NS_IMETHOD InitStyleLinkElement(bool aDontLoadStyle) override;
  NS_IMETHOD UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                              bool* aWillNotify,
                              bool* aIsAlternate,
                              bool aForceReload) override;
  NS_IMETHOD SetEnableUpdates(bool aEnableUpdates) override;
  NS_IMETHOD GetCharset(nsAString& aCharset) override;

  virtual void OverrideBaseURI(nsIURI* aNewBaseURI) override;
  virtual void SetLineNumber(uint32_t aLineNumber) override;
  virtual uint32_t GetLineNumber() override;

  enum RelValue {
    ePREFETCH =     0x00000001,
    eDNS_PREFETCH = 0x00000002,
    eSTYLESHEET =   0x00000004,
    eNEXT =         0x00000008,
    eALTERNATE =    0x00000010,
    eHTMLIMPORT =   0x00000020,
    ePRECONNECT =   0x00000040
  };

  // The return value is a bitwise or of 0 or more RelValues.
  // aPrincipal is used to check if HTML imports is enabled for the
  // provided principal.
  static uint32_t ParseLinkTypes(const nsAString& aTypes,
                                 nsIPrincipal* aPrincipal);

  static bool IsImportEnabled();
  
  void UpdateStyleSheetInternal()
  {
    UpdateStyleSheetInternal(nullptr, nullptr);
  }
protected:
  /**
   * @param aOldDocument should be non-null only if we're updating because we
   *                     removed the node from the document.
   * @param aForceUpdate true will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   */
  nsresult UpdateStyleSheetInternal(nsIDocument *aOldDocument,
                                    mozilla::dom::ShadowRoot *aOldShadowRoot,
                                    bool aForceUpdate = false);

  void UpdateStyleSheetScopedness(bool aIsNowScoped);

  virtual already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) = 0;
  virtual void GetStyleSheetInfo(nsAString& aTitle,
                                 nsAString& aType,
                                 nsAString& aMedia,
                                 bool* aIsScoped,
                                 bool* aIsAlternate) = 0;

  virtual mozilla::CORSMode GetCORSMode() const
  {
    // Default to no CORS
    return mozilla::CORS_NONE;
  }

  virtual mozilla::net::ReferrerPolicy GetLinkReferrerPolicy()
  {
    return mozilla::net::RP_Unset;
  }

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback &cb);

private:
  /**
   * @param aOldDocument should be non-null only if we're updating because we
   *                     removed the node from the document.
   * @param aOldShadowRoot The ShadowRoot that used to contain the style.
   *                     Passed as a parameter because on an update, the node
   *                     is removed from the tree before the sheet is removed
   *                     from the ShadowRoot.
   * @param aForceUpdate true will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   */
  nsresult DoUpdateStyleSheet(nsIDocument* aOldDocument,
                              mozilla::dom::ShadowRoot* aOldShadowRoot,
                              nsICSSLoaderObserver* aObserver,
                              bool* aWillNotify,
                              bool* aIsAlternate,
                              bool aForceUpdate);

  mozilla::StyleSheetHandle::RefPtr mStyleSheet;
protected:
  bool mDontLoadStyle;
  bool mUpdatesEnabled;
  uint32_t mLineNumber;
};

#endif /* nsStyleLinkElement_h___ */

