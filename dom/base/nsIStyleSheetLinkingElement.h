/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIStyleSheetLinkingElement_h__
#define nsIStyleSheetLinkingElement_h__

#include "nsISupports.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/Result.h"

class nsIContent;
class nsICSSLoaderObserver;
class nsIPrincipal;
class nsIURI;

#define NS_ISTYLESHEETLINKINGELEMENT_IID             \
  {                                                  \
    0xa8b79f3b, 0x9d18, 0x4f9c, {                    \
      0xb1, 0xaa, 0x8c, 0x9b, 0x1b, 0xaa, 0xac, 0xad \
    }                                                \
  }

class nsIStyleSheetLinkingElement : public nsISupports {
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

  struct MOZ_STACK_CLASS SheetInfo {
    nsIContent* mContent;
    // FIXME(emilio): do these really need to be strong refs?
    nsCOMPtr<nsIURI> mURI;

    // The principal of the scripted caller that initiated the load, if
    // available. Otherwise null.
    nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
    mozilla::net::ReferrerPolicy mReferrerPolicy;
    mozilla::CORSMode mCORSMode;
    nsString mTitle;
    nsString mMedia;
    nsString mIntegrity;

    bool mHasAlternateRel;
    bool mIsInline;
    IsExplicitlyEnabled mIsExplicitlyEnabled;

    SheetInfo(const mozilla::dom::Document&, nsIContent*,
              already_AddRefed<nsIURI> aURI,
              already_AddRefed<nsIPrincipal> aTriggeringPrincipal,
              mozilla::net::ReferrerPolicy aReferrerPolicy, mozilla::CORSMode,
              const nsAString& aTitle, const nsAString& aMedia, HasAlternateRel,
              IsInline, IsExplicitlyEnabled);

    ~SheetInfo();
  };

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTYLESHEETLINKINGELEMENT_IID)

  /**
   * Used to make the association between a style sheet and
   * the element that linked it to the document.
   *
   * @param aStyleSheet the style sheet associated with this
   *                    element.
   */
  virtual void SetStyleSheet(mozilla::StyleSheet* aStyleSheet) = 0;

  /**
   * Used to obtain the style sheet linked in by this element.
   *
   * @return the style sheet associated with this element.
   */
  virtual mozilla::StyleSheet* GetStyleSheet() = 0;

  /**
   * Initialize the stylesheet linking element. If aDontLoadStyle is
   * true the element will ignore the first modification to the
   * element that would cause a stylesheet to be loaded. Subsequent
   * modifications to the element will not be ignored.
   */
  virtual void InitStyleLinkElement(bool aDontLoadStyle) = 0;

  /**
   * Tells this element to update the stylesheet.
   *
   * @param aObserver    observer to notify once the stylesheet is loaded.
   *                     This will be passed to the CSSLoader
   */
  virtual mozilla::Result<Update, nsresult> UpdateStyleSheet(
      nsICSSLoaderObserver* aObserver) = 0;

  /**
   * Tells this element whether to update the stylesheet when the
   * element's properties change.
   *
   * @param aEnableUpdates update on changes or not.
   */
  virtual void SetEnableUpdates(bool aEnableUpdates) = 0;

  /**
   * Gets the charset that the element claims the style sheet is in.
   * Can return empty string to indicate that we have no charset
   * information.
   *
   * @param aCharset the charset
   */
  virtual void GetCharset(nsAString& aCharset) = 0;

  /**
   * Tells this element to use a different base URI. This is used for
   * proper loading of xml-stylesheet processing instructions in XUL overlays
   * and is only currently used by nsXMLStylesheetPI.
   *
   * @param aNewBaseURI the new base URI, nullptr to use the default base URI.
   */
  virtual void OverrideBaseURI(nsIURI* aNewBaseURI) = 0;

  // This doesn't entirely belong here since they only make sense for
  // some types of linking elements, but it's a better place than
  // anywhere else.
  virtual void SetLineNumber(uint32_t aLineNumber) = 0;

  /**
   * Get the line number, as previously set by SetLineNumber.
   *
   * @return the line number of this element; or 1 if no line number
   *         was set
   */
  virtual uint32_t GetLineNumber() = 0;

  // This doesn't entirely belong here since they only make sense for
  // some types of linking elements, but it's a better place than
  // anywhere else.
  virtual void SetColumnNumber(uint32_t aColumnNumber) = 0;

  /**
   * Get the column number, as previously set by SetColumnNumber.
   *
   * @return the column number of this element; or 1 if no column number
   *         was set
   */
  virtual uint32_t GetColumnNumber() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleSheetLinkingElement,
                              NS_ISTYLESHEETLINKINGELEMENT_IID)

#endif  // nsILinkingElement_h__
