/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rules in a CSS stylesheet other than style rules (e.g., @import rules) */

#ifndef nsCSSRules_h_
#define nsCSSRules_h_

#include "mozilla/Attributes.h"

#include "mozilla/css/GroupRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsIDOMMozCSSKeyframeRule.h"
#include "nsIDOMMozCSSKeyframesRule.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsICSSRuleList.h"
#include "nsAutoPtr.h"
#include "nsCSSProperty.h"
#include "nsCSSValue.h"
#include "nsIDOMCSSCharsetRule.h"
#include "nsTArray.h"
#include "nsDOMCSSDeclaration.h"
#include "Declaration.h"

namespace mozilla {
namespace css {
class StyleRule;
}
}

class nsMediaList;

namespace mozilla {
namespace css {

class MediaRule MOZ_FINAL : public GroupRule,
                            public nsIDOMCSSMediaRule
{
public:
  MediaRule();
private:
  MediaRule(const MediaRule& aCopy);
  ~MediaRule();
public:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // Rule methods
  virtual void SetStyleSheet(nsCSSStyleSheet* aSheet); //override GroupRule
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<Rule> Clone() const;
  virtual nsIDOMCSSRule* GetDOMRule()
  {
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSMediaRule interface
  NS_DECL_NSIDOMCSSMEDIARULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey);

  // @media rule methods
  nsresult SetMedia(nsMediaList* aMedia);
  
  virtual NS_MUST_OVERRIDE size_t
    SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

protected:
  nsRefPtr<nsMediaList> mMedia;
};

class DocumentRule MOZ_FINAL : public GroupRule,
                               public nsIDOMCSSMozDocumentRule
{
public:
  DocumentRule();
private:
  DocumentRule(const DocumentRule& aCopy);
  ~DocumentRule();
public:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // Rule methods
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<Rule> Clone() const;
  virtual nsIDOMCSSRule* GetDOMRule()
  {
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSMozDocumentRule interface
  NS_DECL_NSIDOMCSSMOZDOCUMENTRULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey);

  enum Function {
    eURL,
    eURLPrefix,
    eDomain,
    eRegExp
  };

  struct URL {
    Function func;
    nsCString url;
    URL *next;

    URL() : next(nsnull) {}
    URL(const URL& aOther)
      : func(aOther.func)
      , url(aOther.url)
      , next(aOther.next ? new URL(*aOther.next) : nsnull)
    {
    }
    ~URL();
  };

  void SetURLs(URL *aURLs) { mURLs = aURLs; }

  virtual NS_MUST_OVERRIDE size_t
    SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

protected:
  nsAutoPtr<URL> mURLs; // linked list of |struct URL| above.
};

} // namespace css
} // namespace mozilla

// A nsCSSFontFaceStyleDecl is always embedded in a nsCSSFontFaceRule.
class nsCSSFontFaceRule;
class nsCSSFontFaceStyleDecl : public nsIDOMCSSStyleDeclaration
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCSSSTYLEDECLARATION

  nsresult GetPropertyValue(nsCSSFontDesc aFontDescID,
                            nsAString & aResult NS_OUTPARAM) const;

protected:
  friend class nsCSSFontFaceRule;
#define CSS_FONT_DESC(name_, method_) nsCSSValue m##method_;
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC

  static nsCSSValue nsCSSFontFaceStyleDecl::* const Fields[];  
  inline nsCSSFontFaceRule* ContainingRule();
  inline const nsCSSFontFaceRule* ContainingRule() const;

private:
  // NOT TO BE IMPLEMENTED
  // This object cannot be allocated on its own, only as part of
  // nsCSSFontFaceRule.
  void* operator new(size_t size) CPP_THROW_NEW;
};

class nsCSSFontFaceRule MOZ_FINAL : public mozilla::css::Rule,
                                    public nsIDOMCSSFontFaceRule
{
public:
  nsCSSFontFaceRule() {}

  nsCSSFontFaceRule(const nsCSSFontFaceRule& aCopy)
    // copy everything except our reference count
    : mozilla::css::Rule(aCopy), mDecl(aCopy.mDecl) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // Rule methods
  DECL_STYLE_RULE_INHERIT

  virtual PRInt32 GetType() const;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSFontFaceRule interface
  NS_DECL_NSIDOMCSSFONTFACERULE

  void SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue);
  void GetDesc(nsCSSFontDesc aDescID, nsCSSValue & aValue);

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

protected:
  friend class nsCSSFontFaceStyleDecl;
  nsCSSFontFaceStyleDecl mDecl;
};

// nsFontFaceRuleContainer - used for associating sheet type with 
// specific @font-face rules
struct nsFontFaceRuleContainer {
  nsRefPtr<nsCSSFontFaceRule> mRule;
  PRUint8 mSheetType;
};

inline nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule()
{
  return reinterpret_cast<nsCSSFontFaceRule*>
    (reinterpret_cast<char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

inline const nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule() const
{
  return reinterpret_cast<const nsCSSFontFaceRule*>
    (reinterpret_cast<const char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

namespace mozilla {
namespace css {

class CharsetRule MOZ_FINAL : public Rule,
                              public nsIDOMCSSCharsetRule
{
public:
  CharsetRule(const nsAString& aEncoding);
private:
  // For |Clone|
  CharsetRule(const CharsetRule& aCopy);
  ~CharsetRule() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // Rule methods
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<Rule> Clone() const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSCharsetRule methods
  NS_IMETHOD GetEncoding(nsAString& aEncoding);
  NS_IMETHOD SetEncoding(const nsAString& aEncoding);

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  nsString  mEncoding;
};

} // namespace css
} // namespace mozilla

class nsCSSKeyframeRule;

class nsCSSKeyframeStyleDeclaration MOZ_FINAL : public nsDOMCSSDeclaration
{
public:
  nsCSSKeyframeStyleDeclaration(nsCSSKeyframeRule *aRule);
  virtual ~nsCSSKeyframeStyleDeclaration();

  NS_IMETHOD GetParentRule(nsIDOMCSSRule **aParent);
  void DropReference() { mRule = nsnull; }
  virtual mozilla::css::Declaration* GetCSSDeclaration(bool aAllocate);
  virtual nsresult SetCSSDeclaration(mozilla::css::Declaration* aDecl);
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv);
  virtual nsIDocument* DocToUpdate();

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual nsINode *GetParentObject()
  {
    return nsnull;
  }

protected:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // This reference is not reference-counted. The rule object tells us
  // when it's about to go away.
  nsCSSKeyframeRule *mRule;
};

class nsCSSKeyframeRule MOZ_FINAL : public mozilla::css::Rule,
                                    public nsIDOMMozCSSKeyframeRule
{
public:
  // WARNING: Steals the contents of aKeys *and* aDeclaration
  nsCSSKeyframeRule(nsTArray<float> aKeys,
                    nsAutoPtr<mozilla::css::Declaration> aDeclaration)
    : mDeclaration(aDeclaration)
  {
    mKeys.SwapElements(aKeys);
  }
private:
  nsCSSKeyframeRule(const nsCSSKeyframeRule& aCopy);
  ~nsCSSKeyframeRule();
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // Rule methods
  DECL_STYLE_RULE_INHERIT
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMMozCSSKeyframeRule interface
  NS_DECL_NSIDOMMOZCSSKEYFRAMERULE

  const nsTArray<float>& GetKeys() const     { return mKeys; }
  mozilla::css::Declaration* Declaration()   { return mDeclaration; }

  void ChangeDeclaration(mozilla::css::Declaration* aDeclaration);

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  nsAutoTArray<float, 1>                     mKeys;
  nsAutoPtr<mozilla::css::Declaration>       mDeclaration;
  // lazily created when needed:
  nsRefPtr<nsCSSKeyframeStyleDeclaration>    mDOMDeclaration;
};

class nsCSSKeyframesRule MOZ_FINAL : public mozilla::css::GroupRule,
                                     public nsIDOMMozCSSKeyframesRule
{
public:
  nsCSSKeyframesRule(const nsSubstring& aName)
    : mName(aName)
  {
  }
private:
  nsCSSKeyframesRule(const nsCSSKeyframesRule& aCopy);
  ~nsCSSKeyframesRule();
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // Rule methods
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const;
  virtual nsIDOMCSSRule* GetDOMRule()
  {
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMMozCSSKeyframesRule interface
  NS_DECL_NSIDOMMOZCSSKEYFRAMESRULE

  // rest of GroupRule
  virtual bool UseForPresentation(nsPresContext* aPresContext,
                                    nsMediaQueryResultCacheKey& aKey);

  const nsString& GetName() { return mName; }

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  PRUint32 FindRuleIndexForKey(const nsAString& aKey);

  nsString                                   mName;
};

#endif /* !defined(nsCSSRules_h_) */
