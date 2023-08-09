/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_LinkStyle_h
#define mozilla_dom_LinkStyle_h

#include "nsINode.h"
#include "mozilla/Attributes.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/Result.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"

class nsIContent;
class nsICSSLoaderObserver;
class nsIPrincipal;
class nsIURI;

namespace mozilla::dom {

class Document;
class ShadowRoot;

// https://drafts.csswg.org/cssom/#the-linkstyle-interface
class LinkStyle {
 public:
  enum class ForceUpdate : uint8_t {
    No,
    Yes,
  };

  enum class Completed : uint8_t {
    No,
    Yes,
  };

  enum class HasAlternateRel : uint8_t {
    No,
    Yes,
  };

  enum class IsAlternate : uint8_t {
    No,
    Yes,
  };

  enum class IsInline : uint8_t {
    No,
    Yes,
  };

  enum class IsExplicitlyEnabled : uint8_t {
    No,
    Yes,
  };

  enum class MediaMatched : uint8_t {
    Yes,
    No,
  };

  struct Update {
   private:
    bool mWillNotify;
    bool mIsAlternate;
    bool mMediaMatched;

   public:
    Update() : mWillNotify(false), mIsAlternate(false), mMediaMatched(false) {}

    Update(Completed aCompleted, IsAlternate aIsAlternate,
           MediaMatched aMediaMatched)
        : mWillNotify(aCompleted == Completed::No),
          mIsAlternate(aIsAlternate == IsAlternate::Yes),
          mMediaMatched(aMediaMatched == MediaMatched::Yes) {}

    bool WillNotify() const { return mWillNotify; }

    bool ShouldBlock() const {
      if (!mWillNotify) {
        return false;
      }

      return !mIsAlternate && mMediaMatched;
    }
  };

  static LinkStyle* FromNode(nsINode& aNode) { return aNode.AsLinkStyle(); }
  static const LinkStyle* FromNode(const nsINode& aNode) {
    return aNode.AsLinkStyle();
  }

  static LinkStyle* FromNode(Element&);
  static const LinkStyle* FromNode(const Element& aElement) {
    return FromNode(const_cast<Element&>(aElement));
  }

  template <typename T>
  static LinkStyle* FromNodeOrNull(T* aNode) {
    return aNode ? FromNode(*aNode) : nullptr;
  }

  template <typename T>
  static const LinkStyle* FromNodeOrNull(const T* aNode) {
    return aNode ? FromNode(*aNode) : nullptr;
  }

  enum RelValue {
    ePREFETCH = 0x00000001,
    eDNS_PREFETCH = 0x00000002,
    eSTYLESHEET = 0x00000004,
    eNEXT = 0x00000008,
    eALTERNATE = 0x00000010,
    ePRECONNECT = 0x00000020,
    // NOTE: 0x40 is unused
    ePRELOAD = 0x00000080,
    eMODULE_PRELOAD = 0x00000100
  };

  // The return value is a bitwise or of 0 or more RelValues.
  static uint32_t ParseLinkTypes(const nsAString& aTypes);

  void UpdateStyleSheetInternal() {
    Unused << UpdateStyleSheetInternal(nullptr, nullptr);
  }

  struct MOZ_STACK_CLASS SheetInfo {
    nsIContent* mContent;
    // FIXME(emilio): do these really need to be strong refs?
    nsCOMPtr<nsIURI> mURI;

    // The principal of the scripted caller that initiated the load, if
    // available. Otherwise null.
    nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
    nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
    mozilla::CORSMode mCORSMode;
    nsString mTitle;
    nsString mMedia;
    nsString mIntegrity;
    nsString mNonce;

    bool mHasAlternateRel;
    bool mIsInline;
    IsExplicitlyEnabled mIsExplicitlyEnabled;

    SheetInfo(const mozilla::dom::Document&, nsIContent*,
              already_AddRefed<nsIURI> aURI,
              already_AddRefed<nsIPrincipal> aTriggeringPrincipal,
              already_AddRefed<nsIReferrerInfo> aReferrerInfo,
              mozilla::CORSMode, const nsAString& aTitle,
              const nsAString& aMedia, const nsAString& aIntegrity,
              const nsAString& aNonce, HasAlternateRel, IsInline,
              IsExplicitlyEnabled);

    ~SheetInfo();
  };

  virtual nsIContent& AsContent() = 0;
  virtual Maybe<SheetInfo> GetStyleSheetInfo() = 0;

  /**
   * Used to make the association between a style sheet and
   * the element that linked it to the document.
   *
   * @param aStyleSheet the style sheet associated with this
   *                    element.
   */
  void SetStyleSheet(StyleSheet* aStyleSheet);

  /**
   * Tells this element whether to update the stylesheet when the element's
   * properties change. This is used by the parser until it has all content etc,
   * and to guarantee that the right observer is used.
   */
  void DisableUpdates() { mUpdatesEnabled = false; }
  Result<Update, nsresult> EnableUpdatesAndUpdateStyleSheet(
      nsICSSLoaderObserver* aObserver) {
    MOZ_ASSERT(!mUpdatesEnabled);
    mUpdatesEnabled = true;
    return DoUpdateStyleSheet(nullptr, nullptr, aObserver, ForceUpdate::No);
  }

  /**
   * Gets the charset that the element claims the style sheet is in.
   * Can return empty string to indicate that we have no charset
   * information.
   *
   * @param aCharset the charset
   */
  virtual void GetCharset(nsAString& aCharset);

  // This doesn't entirely belong here since they only make sense for
  // some types of linking elements, but it's a better place than
  // anywhere else.
  void SetLineNumber(uint32_t aLineNumber) { mLineNumber = aLineNumber; }

  /**
   * Get the line number, as previously set by SetLineNumber.
   *
   * @return the line number of this element; or 1 if no line number
   *         was set
   */
  uint32_t GetLineNumber() const { return mLineNumber; }

  // This doesn't entirely belong here since they only make sense for
  // some types of linking elements, but it's a better place than
  // anywhere else.
  void SetColumnNumber(uint32_t aColumnNumber) {
    mColumnNumber = aColumnNumber;
  }

  /**
   * Get the column number, as previously set by SetColumnNumber.
   *
   * @return the column number of this element; or 1 if no column number
   *         was set
   */
  uint32_t GetColumnNumber() const { return mColumnNumber; }

  StyleSheet* GetSheet() const { return mStyleSheet; }

  /** JS can only observe the sheet once fully loaded */
  StyleSheet* GetSheetForBindings() const;

 protected:
  LinkStyle();
  virtual ~LinkStyle();

  // Gets a suitable title and media for SheetInfo out of an element, which
  // needs to be `this`.
  //
  // NOTE(emilio): Needs nsString instead of nsAString because of
  // CompressWhitespace.
  static void GetTitleAndMediaForElement(const mozilla::dom::Element&,
                                         nsString& aTitle, nsString& aMedia);

  // Returns whether the type attribute specifies the text/css type for style
  // elements.
  static bool IsCSSMimeTypeAttributeForStyleElement(const Element&);

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback& cb);

  /**
   * @param aOldDocument   should be non-null only if we're updating because we
   *                       removed the node from the document.
   * @param aOldShadowRoot should be non-null only if we're updating because we
   *                       removed the node from a shadow tree.
   * @param aForceUpdate true will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   *
   * TODO(emilio): Should probably pass a single DocumentOrShadowRoot.
   */
  Result<Update, nsresult> UpdateStyleSheetInternal(
      Document* aOldDocument, ShadowRoot* aOldShadowRoot,
      ForceUpdate = ForceUpdate::No);

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
  Result<Update, nsresult> DoUpdateStyleSheet(Document* aOldDocument,
                                              ShadowRoot* aOldShadowRoot,
                                              nsICSSLoaderObserver*,
                                              ForceUpdate);

  void BindToTree();

  RefPtr<mozilla::StyleSheet> mStyleSheet;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  bool mUpdatesEnabled = true;
  uint32_t mLineNumber = 1;
  uint32_t mColumnNumber = 1;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_LinkStyle_h
