/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheet_h
#define mozilla_StyleSheet_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/dom/CSSStyleSheetBinding.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/CORSMode.h"
#include "mozilla/ServoUtils.h"

#include "nsIDOMCSSStyleSheet.h"
#include "nsWrapperCache.h"

class nsIDocument;
class nsINode;
class nsIPrincipal;
class nsMediaList;

namespace mozilla {

class CSSStyleSheet;
class ServoStyleSheet;
struct StyleSheetInfo;

namespace dom {
class CSSRuleList;
class SRIMetadata;
} // namespace dom

/**
 * Superclass for data common to CSSStyleSheet and ServoStyleSheet.
 */
class StyleSheet : public nsIDOMCSSStyleSheet
                 , public nsWrapperCache
{
protected:
  StyleSheet(StyleBackendType aType, css::SheetParsingMode aParsingMode);
  StyleSheet(const StyleSheet& aCopy,
             nsIDocument* aDocumentToUse,
             nsINode* aOwningNodeToUse);
  virtual ~StyleSheet();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(StyleSheet)

  void SetOwningNode(nsINode* aOwningNode)
  {
    mOwningNode = aOwningNode;
  }

  css::SheetParsingMode ParsingMode() { return mParsingMode; }
  mozilla::dom::CSSStyleSheetParsingMode ParsingModeDOM();

  // The document this style sheet is associated with.  May be null
  nsIDocument* GetDocument() const { return mDocument; }

  /**
   * Whether the sheet is complete.
   */
  bool IsComplete() const;
  void SetComplete();

  /**
   * Set the stylesheet to be enabled.  This may or may not make it
   * applicable.  Note that this WILL inform the sheet's document of
   * its new applicable state if the state changes but WILL NOT call
   * BeginUpdate() or EndUpdate() on the document -- calling those is
   * the caller's responsibility.  This allows use of SetEnabled when
   * batched updates are desired.  If you want updates handled for
   * you, see nsIDOMStyleSheet::SetDisabled().
   */
  void SetEnabled(bool aEnabled);

  MOZ_DECL_STYLO_METHODS(CSSStyleSheet, ServoStyleSheet)

  // Whether the sheet is for an inline <style> element.
  inline bool IsInline() const;

  inline nsIURI* GetSheetURI() const;
  /* Get the URI this sheet was originally loaded from, if any.  Can
     return null */
  inline nsIURI* GetOriginalURI() const;
  inline nsIURI* GetBaseURI() const;
  /**
   * SetURIs must be called on all sheets before parsing into them.
   * SetURIs may only be called while the sheet is 1) incomplete and 2)
   * has no rules in it
   */
  inline void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                      nsIURI* aBaseURI);

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   */
  inline bool IsApplicable() const;
  inline bool HasRules() const;

  // style sheet owner info
  nsIDocument* GetOwningDocument() const { return mDocument; }
  inline void SetOwningDocument(nsIDocument* aDocument);
  nsINode* GetOwnerNode() const { return mOwningNode; }
  inline StyleSheet* GetParentSheet() const;

  inline void AppendStyleSheet(StyleSheet* aSheet);

  // Principal() never returns a null pointer.
  inline nsIPrincipal* Principal() const;
  /**
   * SetPrincipal should be called on all sheets before parsing into them.
   * This can only be called once with a non-null principal.  Calling this with
   * a null pointer is allowed and is treated as a no-op.
   */
  inline void SetPrincipal(nsIPrincipal* aPrincipal);

  void SetTitle(const nsAString& aTitle) { mTitle = aTitle; }
  void SetMedia(nsMediaList* aMedia);

  // Get this style sheet's CORS mode
  inline CORSMode GetCORSMode() const;
  // Get this style sheet's Referrer Policy
  inline net::ReferrerPolicy GetReferrerPolicy() const;
  // Get this style sheet's integrity metadata
  inline void GetIntegrity(dom::SRIMetadata& aResult) const;

  inline size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
#ifdef DEBUG
  inline void List(FILE* aOut = stdout, int32_t aIndex = 0) const;
#endif

  // WebIDL StyleSheet API
  // The XPCOM GetType is fine for WebIDL.
  // The XPCOM GetHref is fine for WebIDL
  // GetOwnerNode is defined above.
  inline StyleSheet* GetParentStyleSheet() const;
  // The XPCOM GetTitle is fine for WebIDL.
  nsMediaList* Media();
  bool Disabled() const { return mDisabled; }
  // The XPCOM SetDisabled is fine for WebIDL.

  // WebIDL CSSStyleSheet API
  virtual nsIDOMCSSRule* GetDOMOwnerRule() const = 0;
  dom::CSSRuleList* GetCssRules(nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aRv);
  uint32_t InsertRule(const nsAString& aRule, uint32_t aIndex,
                      nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv);
  void DeleteRule(uint32_t aIndex,
                  nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv);

  // WebIDL miscellaneous bits
  inline dom::ParentObject GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  // nsIDOMStyleSheet interface
  NS_IMETHOD GetType(nsAString& aType) final;
  NS_IMETHOD GetDisabled(bool* aDisabled) final;
  NS_IMETHOD SetDisabled(bool aDisabled) final;
  NS_IMETHOD GetOwnerNode(nsIDOMNode** aOwnerNode) final;
  NS_IMETHOD GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet) final;
  NS_IMETHOD GetHref(nsAString& aHref) final;
  NS_IMETHOD GetTitle(nsAString& aTitle) final;
  NS_IMETHOD GetMedia(nsIDOMMediaList** aMedia) final;

  // nsIDOMCSSStyleSheet
  NS_IMETHOD GetOwnerRule(nsIDOMCSSRule** aOwnerRule) final;
  NS_IMETHOD GetCssRules(nsIDOMCSSRuleList** aCssRules) final;
  NS_IMETHOD InsertRule(const nsAString& aRule, uint32_t aIndex,
                      uint32_t* aReturn) final;
  NS_IMETHOD DeleteRule(uint32_t aIndex) final;

  // Changes to sheets should be inside of a WillDirty-DidDirty pair.
  // However, the calls do not need to be matched; it's ok to call
  // WillDirty and then make no change and skip the DidDirty call.
  inline void WillDirty();
  inline void DidDirty();

private:
  // Get a handle to the various stylesheet bits which live on the 'inner' for
  // gecko stylesheets and live on the StyleSheet for Servo stylesheets.
  inline StyleSheetInfo& SheetInfo();
  inline const StyleSheetInfo& SheetInfo() const;

  // Check if the rules are available for read and write.
  // It does the security check as well as whether the rules have been
  // completely loaded. aRv will have an exception set if this function
  // returns false.
  bool AreRulesAvailable(nsIPrincipal& aSubjectPrincipal,
                         ErrorResult& aRv);

protected:
  // Return success if the subject principal subsumes the principal of our
  // inner, error otherwise.  This will also succeed if the subject has
  // UniversalXPConnect or if access is allowed by CORS.  In the latter case,
  // it will set the principal of the inner to the subject principal.
  void SubjectSubsumesInnerPrincipal(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aRv);

  // Drop our reference to mMedia
  void DropMedia();

  // Called from SetEnabled when the enabled state changed.
  void EnabledStateChanged();

  nsString              mTitle;
  nsIDocument*          mDocument; // weak ref; parents maintain this for their children
  nsINode*              mOwningNode; // weak ref

  RefPtr<nsMediaList> mMedia;

  // mParsingMode controls access to nonstandard style constructs that
  // are not safe for use on the public Web but necessary in UA sheets
  // and/or useful in user sheets.
  css::SheetParsingMode mParsingMode;

  const StyleBackendType mType;
  bool                  mDisabled;
};

} // namespace mozilla

#endif // mozilla_StyleSheet_h
