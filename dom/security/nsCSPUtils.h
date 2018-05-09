/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPUtils_h___
#define nsCSPUtils_h___

#include "nsCOMPtr.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace dom {
  struct CSP;
} // namespace dom
} // namespace mozilla

/* =============== Logging =================== */

void CSP_LogLocalizedStr(const char* aName,
                         const char16_t** aParams,
                         uint32_t aLength,
                         const nsAString& aSourceName,
                         const nsAString& aSourceLine,
                         uint32_t aLineNumber,
                         uint32_t aColumnNumber,
                         uint32_t aFlags,
                         const char* aCategory,
                         uint64_t aInnerWindowID,
                         bool aFromPrivateWindow);

void CSP_GetLocalizedStr(const char* aName,
                         const char16_t** aParams,
                         uint32_t aLength,
                         nsAString& outResult);

void CSP_LogStrMessage(const nsAString& aMsg);

void CSP_LogMessage(const nsAString& aMessage,
                    const nsAString& aSourceName,
                    const nsAString& aSourceLine,
                    uint32_t aLineNumber,
                    uint32_t aColumnNumber,
                    uint32_t aFlags,
                    const char* aCategory,
                    uint64_t aInnerWindowID,
                    bool aFromPrivateWindow);


/* =============== Constant and Type Definitions ================== */

#define INLINE_STYLE_VIOLATION_OBSERVER_TOPIC        "violated base restriction: Inline Stylesheets will not apply"
#define INLINE_SCRIPT_VIOLATION_OBSERVER_TOPIC       "violated base restriction: Inline Scripts will not execute"
#define EVAL_VIOLATION_OBSERVER_TOPIC                "violated base restriction: Code will not be created from strings"
#define SCRIPT_NONCE_VIOLATION_OBSERVER_TOPIC        "Inline Script had invalid nonce"
#define STYLE_NONCE_VIOLATION_OBSERVER_TOPIC         "Inline Style had invalid nonce"
#define SCRIPT_HASH_VIOLATION_OBSERVER_TOPIC         "Inline Script had invalid hash"
#define STYLE_HASH_VIOLATION_OBSERVER_TOPIC          "Inline Style had invalid hash"
#define REQUIRE_SRI_SCRIPT_VIOLATION_OBSERVER_TOPIC  "Missing required Subresource Integrity for Script"
#define REQUIRE_SRI_STYLE_VIOLATION_OBSERVER_TOPIC   "Missing required Subresource Integrity for Style"

// these strings map to the CSPDirectives in nsIContentSecurityPolicy
// NOTE: When implementing a new directive, you will need to add it here but also
// add a corresponding entry to the constants in nsIContentSecurityPolicy.idl
// and also create an entry for the new directive in
// nsCSPDirective::toDomCSPStruct() and add it to CSPDictionaries.webidl.
// Order of elements below important! Make sure it matches the order as in
// nsIContentSecurityPolicy.idl
static const char* CSPStrDirectives[] = {
  "-error-",                   // NO_DIRECTIVE
  "default-src",               // DEFAULT_SRC_DIRECTIVE
  "script-src",                // SCRIPT_SRC_DIRECTIVE
  "object-src",                // OBJECT_SRC_DIRECTIVE
  "style-src",                 // STYLE_SRC_DIRECTIVE
  "img-src",                   // IMG_SRC_DIRECTIVE
  "media-src",                 // MEDIA_SRC_DIRECTIVE
  "frame-src",                 // FRAME_SRC_DIRECTIVE
  "font-src",                  // FONT_SRC_DIRECTIVE
  "connect-src",               // CONNECT_SRC_DIRECTIVE
  "report-uri",                // REPORT_URI_DIRECTIVE
  "frame-ancestors",           // FRAME_ANCESTORS_DIRECTIVE
  "reflected-xss",             // REFLECTED_XSS_DIRECTIVE
  "base-uri",                  // BASE_URI_DIRECTIVE
  "form-action",               // FORM_ACTION_DIRECTIVE
  "manifest-src",              // MANIFEST_SRC_DIRECTIVE
  "upgrade-insecure-requests", // UPGRADE_IF_INSECURE_DIRECTIVE
  "child-src",                 // CHILD_SRC_DIRECTIVE
  "block-all-mixed-content",   // BLOCK_ALL_MIXED_CONTENT
  "require-sri-for",           // REQUIRE_SRI_FOR
  "sandbox",                   // SANDBOX_DIRECTIVE
  "worker-src"                 // WORKER_SRC_DIRECTIVE
};

inline const char* CSP_CSPDirectiveToString(CSPDirective aDir)
{
  return CSPStrDirectives[static_cast<uint32_t>(aDir)];
}

inline CSPDirective CSP_StringToCSPDirective(const nsAString& aDir)
{
  nsString lowerDir = PromiseFlatString(aDir);
  ToLowerCase(lowerDir);

  uint32_t numDirs = (sizeof(CSPStrDirectives) / sizeof(CSPStrDirectives[0]));
  for (uint32_t i = 1; i < numDirs; i++) {
    if (lowerDir.EqualsASCII(CSPStrDirectives[i])) {
      return static_cast<CSPDirective>(i);
    }
  }
  NS_ASSERTION(false, "Can not convert unknown Directive to Integer");
  return nsIContentSecurityPolicy::NO_DIRECTIVE;
}

#define FOR_EACH_CSP_KEYWORD(macro) \
  macro(CSP_SELF,            "'self'") \
  macro(CSP_UNSAFE_INLINE,   "'unsafe-inline'") \
  macro(CSP_UNSAFE_EVAL,     "'unsafe-eval'") \
  macro(CSP_NONE,            "'none'") \
  macro(CSP_NONCE,           "'nonce-") \
  macro(CSP_REQUIRE_SRI_FOR, "require-sri-for") \
  macro(CSP_STRICT_DYNAMIC,  "'strict-dynamic'")

enum CSPKeyword {
  #define KEYWORD_ENUM(id_, string_) id_,
  FOR_EACH_CSP_KEYWORD(KEYWORD_ENUM)
  #undef KEYWORD_ENUM

  // CSP_LAST_KEYWORD_VALUE always needs to be the last element in the enum
  // because we use it to calculate the size for the char* array.
  CSP_LAST_KEYWORD_VALUE,

  // Putting CSP_HASH after the delimitor, because CSP_HASH is not a valid
  // keyword (hash uses e.g. sha256, sha512) but we use CSP_HASH internally
  // to identify allowed hashes in ::allows.
  CSP_HASH
 };

// The keywords, in UTF-8 form.
static const char* gCSPUTF8Keywords[] = {
  #define KEYWORD_UTF8_LITERAL(id_, string_) string_,
  FOR_EACH_CSP_KEYWORD(KEYWORD_UTF8_LITERAL)
  #undef KEYWORD_UTF8_LITERAL
};

// The keywords, in UTF-16 form.
static const char16_t* gCSPUTF16Keywords[] = {
  #define KEYWORD_UTF16_LITERAL(id_, string_) u"" string_,
  FOR_EACH_CSP_KEYWORD(KEYWORD_UTF16_LITERAL)
  #undef KEYWORD_UTF16_LITERAL
};

#undef FOR_EACH_CSP_KEYWORD

inline const char* CSP_EnumToUTF8Keyword(enum CSPKeyword aKey)
{
  // Make sure all elements in enum CSPKeyword got added to gCSPUTF8Keywords.
  static_assert((sizeof(gCSPUTF8Keywords) / sizeof(gCSPUTF8Keywords[0]) ==
                CSP_LAST_KEYWORD_VALUE),
                "CSP_LAST_KEYWORD_VALUE != length(gCSPUTF8Keywords)");

  if (static_cast<uint32_t>(aKey) <
      static_cast<uint32_t>(CSP_LAST_KEYWORD_VALUE)) {
    return gCSPUTF8Keywords[static_cast<uint32_t>(aKey)];
  }
  return "error: invalid keyword in CSP_EnumToUTF8Keyword";
}

inline const char16_t* CSP_EnumToUTF16Keyword(enum CSPKeyword aKey)
{
  // Make sure all elements in enum CSPKeyword got added to gCSPUTF16Keywords.
  static_assert((sizeof(gCSPUTF16Keywords) / sizeof(gCSPUTF16Keywords[0]) ==
                CSP_LAST_KEYWORD_VALUE),
                "CSP_LAST_KEYWORD_VALUE != length(gCSPUTF16Keywords)");

  if (static_cast<uint32_t>(aKey) <
      static_cast<uint32_t>(CSP_LAST_KEYWORD_VALUE)) {
    return gCSPUTF16Keywords[static_cast<uint32_t>(aKey)];
  }
  return u"error: invalid keyword in CSP_EnumToUTF16Keyword";
}

inline CSPKeyword CSP_UTF16KeywordToEnum(const nsAString& aKey)
{
  nsString lowerKey = PromiseFlatString(aKey);
  ToLowerCase(lowerKey);

  for (uint32_t i = 0; i < CSP_LAST_KEYWORD_VALUE; i++) {
    if (lowerKey.Equals(gCSPUTF16Keywords[i])) {
      return static_cast<CSPKeyword>(i);
    }
  }
  NS_ASSERTION(false, "Can not convert unknown Keyword to Enum");
  return CSP_LAST_KEYWORD_VALUE;
}

nsresult CSP_AppendCSPFromHeader(nsIContentSecurityPolicy* aCsp,
                                 const nsAString& aHeaderValue,
                                 bool aReportOnly);

/* =============== Helpers ================== */

class nsCSPHostSrc;

nsCSPHostSrc* CSP_CreateHostSrcFromSelfURI(nsIURI* aSelfURI);
bool CSP_IsEmptyDirective(const nsAString& aValue, const nsAString& aDir);
bool CSP_IsValidDirective(const nsAString& aDir);
bool CSP_IsDirective(const nsAString& aValue, CSPDirective aDir);
bool CSP_IsKeyword(const nsAString& aValue, enum CSPKeyword aKey);
bool CSP_IsQuotelessKeyword(const nsAString& aKey);
CSPDirective CSP_ContentTypeToDirective(nsContentPolicyType aType);

class nsCSPSrcVisitor;

void CSP_PercentDecodeStr(const nsAString& aEncStr, nsAString& outDecStr);

/* =============== nsCSPSrc ================== */

class nsCSPBaseSrc {
  public:
    nsCSPBaseSrc();
    virtual ~nsCSPBaseSrc();

    virtual bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                         bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const;
    virtual bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                        bool aParserCreated) const;
    virtual bool visit(nsCSPSrcVisitor* aVisitor) const = 0;
    virtual void toString(nsAString& outStr) const = 0;

    virtual void invalidate() const
      { mInvalidated = true; }

  protected:
    // invalidate srcs if 'script-dynamic' is present or also invalidate
    // unsafe-inline' if nonce- or hash-source specified
    mutable bool mInvalidated;

};

/* =============== nsCSPSchemeSrc ============ */

class nsCSPSchemeSrc : public nsCSPBaseSrc {
  public:
    explicit nsCSPSchemeSrc(const nsAString& aScheme);
    virtual ~nsCSPSchemeSrc();

    bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                 bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const override;
    bool visit(nsCSPSrcVisitor* aVisitor) const override;
    void toString(nsAString& outStr) const override;

    inline void getScheme(nsAString& outStr) const
      { outStr.Assign(mScheme); };

  private:
    nsString mScheme;
};

/* =============== nsCSPHostSrc ============== */

class nsCSPHostSrc : public nsCSPBaseSrc {
  public:
    explicit nsCSPHostSrc(const nsAString& aHost);
    virtual ~nsCSPHostSrc();

    bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                 bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const override;
    bool visit(nsCSPSrcVisitor* aVisitor) const override;
    void toString(nsAString& outStr) const override;

    void setScheme(const nsAString& aScheme);
    void setPort(const nsAString& aPort);
    void appendPath(const nsAString &aPath);

    inline void setGeneratedFromSelfKeyword() const
      { mGeneratedFromSelfKeyword = true; }

    inline void setIsUniqueOrigin() const
      { mIsUniqueOrigin = true; }

    inline void setWithinFrameAncestorsDir(bool aValue) const
      { mWithinFrameAncstorsDir = aValue; }

    inline void getScheme(nsAString& outStr) const
      { outStr.Assign(mScheme); };

    inline void getHost(nsAString& outStr) const
      { outStr.Assign(mHost); };

    inline void getPort(nsAString& outStr) const
      { outStr.Assign(mPort); };

    inline void getPath(nsAString& outStr) const
      { outStr.Assign(mPath); };

  private:
    nsString mScheme;
    nsString mHost;
    nsString mPort;
    nsString mPath;
    mutable bool mGeneratedFromSelfKeyword;
    mutable bool mIsUniqueOrigin;
    mutable bool mWithinFrameAncstorsDir;
};

/* =============== nsCSPKeywordSrc ============ */

class nsCSPKeywordSrc : public nsCSPBaseSrc {
  public:
    explicit nsCSPKeywordSrc(CSPKeyword aKeyword);
    virtual ~nsCSPKeywordSrc();

    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                bool aParserCreated) const override;
    bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                 bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const override;
    bool visit(nsCSPSrcVisitor* aVisitor) const override;
    void toString(nsAString& outStr) const override;

    inline CSPKeyword getKeyword() const
      { return mKeyword; };

    inline void invalidate() const override
    {
      // keywords that need to invalidated
      if (mKeyword == CSP_SELF || mKeyword == CSP_UNSAFE_INLINE) {
        mInvalidated = true;
      }
    }

  private:
    CSPKeyword mKeyword;
};

/* =============== nsCSPNonceSource =========== */

class nsCSPNonceSrc : public nsCSPBaseSrc {
  public:
    explicit nsCSPNonceSrc(const nsAString& aNonce);
    virtual ~nsCSPNonceSrc();

    bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                 bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const override;
    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                bool aParserCreated) const override;
    bool visit(nsCSPSrcVisitor* aVisitor) const override;
    void toString(nsAString& outStr) const override;

    inline void getNonce(nsAString& outStr) const
      { outStr.Assign(mNonce); };

    inline void invalidate() const override
    {
      // overwrite nsCSPBaseSRC::invalidate() and explicitily
      // do *not* invalidate, because 'strict-dynamic' should
      // not invalidate nonces.
    }

  private:
    nsString mNonce;
};

/* =============== nsCSPHashSource ============ */

class nsCSPHashSrc : public nsCSPBaseSrc {
  public:
    nsCSPHashSrc(const nsAString& algo, const nsAString& hash);
    virtual ~nsCSPHashSrc();

    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                bool aParserCreated) const override;
    void toString(nsAString& outStr) const override;
    bool visit(nsCSPSrcVisitor* aVisitor) const override;

    inline void getAlgorithm(nsAString& outStr) const
      { outStr.Assign(mAlgorithm); };

    inline void getHash(nsAString& outStr) const
      { outStr.Assign(mHash); };

    inline void invalidate() const override
    {
      // overwrite nsCSPBaseSRC::invalidate() and explicitily
      // do *not* invalidate, because 'strict-dynamic' should
      // not invalidate hashes.
    }

  private:
    nsString mAlgorithm;
    nsString mHash;
};

/* =============== nsCSPReportURI ============ */

class nsCSPReportURI : public nsCSPBaseSrc {
  public:
    explicit nsCSPReportURI(nsIURI* aURI);
    virtual ~nsCSPReportURI();

    bool visit(nsCSPSrcVisitor* aVisitor) const override;
    void toString(nsAString& outStr) const override;

  private:
    nsCOMPtr<nsIURI> mReportURI;
};

/* =============== nsCSPSandboxFlags ================== */

class nsCSPSandboxFlags : public nsCSPBaseSrc {
  public:
    explicit nsCSPSandboxFlags(const nsAString& aFlags);
    virtual ~nsCSPSandboxFlags();

    bool visit(nsCSPSrcVisitor* aVisitor) const override;
    void toString(nsAString& outStr) const override;

  private:
    nsString mFlags;
};

/* =============== nsCSPSrcVisitor ================== */

class nsCSPSrcVisitor {
  public:
    virtual bool visitSchemeSrc(const nsCSPSchemeSrc& src) = 0;

    virtual bool visitHostSrc(const nsCSPHostSrc& src) = 0;

    virtual bool visitKeywordSrc(const nsCSPKeywordSrc& src) = 0;

    virtual bool visitNonceSrc(const nsCSPNonceSrc& src) = 0;

    virtual bool visitHashSrc(const nsCSPHashSrc& src) = 0;

  protected:
    explicit nsCSPSrcVisitor() {};
    virtual ~nsCSPSrcVisitor() {};
};

/* =============== nsCSPDirective ============= */

class nsCSPDirective {
  public:
    explicit nsCSPDirective(CSPDirective aDirective);
    virtual ~nsCSPDirective();

    virtual bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                         bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const;
    virtual bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                 bool aParserCreated) const;
    virtual void toString(nsAString& outStr) const;
    void toDomCSPStruct(mozilla::dom::CSP& outCSP) const;

    virtual void addSrcs(const nsTArray<nsCSPBaseSrc*>& aSrcs)
      { mSrcs = aSrcs; }

    virtual bool restrictsContentType(nsContentPolicyType aContentType) const;

    inline bool isDefaultDirective() const
     { return mDirective == nsIContentSecurityPolicy::DEFAULT_SRC_DIRECTIVE; }

    virtual bool equals(CSPDirective aDirective) const;

    void getReportURIs(nsTArray<nsString> &outReportURIs) const;

    bool visitSrcs(nsCSPSrcVisitor* aVisitor) const;

    virtual void getDirName(nsAString& outStr) const;

  protected:
    CSPDirective            mDirective;
    nsTArray<nsCSPBaseSrc*> mSrcs;
};

/* =============== nsCSPChildSrcDirective ============= */

/*
 * In CSP 3 child-src is deprecated. For backwards compatibility
 * child-src needs to restrict:
 *   (*) frames, in case frame-src is not expicitly specified
 *   (*) workers, in case worker-src is not expicitly specified
 */
class nsCSPChildSrcDirective : public nsCSPDirective {
  public:
    explicit nsCSPChildSrcDirective(CSPDirective aDirective);
    virtual ~nsCSPChildSrcDirective();

    void setRestrictFrames()
      { mRestrictFrames = true; }

    void setRestrictWorkers()
      { mRestrictWorkers = true; }

    virtual bool restrictsContentType(nsContentPolicyType aContentType) const override;

    virtual bool equals(CSPDirective aDirective) const override;

  private:
    bool mRestrictFrames;
    bool mRestrictWorkers;
};

/* =============== nsCSPScriptSrcDirective ============= */

/*
 * In CSP 3 worker-src restricts workers, for backwards compatibily
 * script-src has to restrict workers as the ultimate fallback if
 * neither worker-src nor child-src is present in a CSP.
 */
class nsCSPScriptSrcDirective : public nsCSPDirective {
  public:
    explicit nsCSPScriptSrcDirective(CSPDirective aDirective);
    virtual ~nsCSPScriptSrcDirective();

    void setRestrictWorkers()
      { mRestrictWorkers = true; }

    virtual bool restrictsContentType(nsContentPolicyType aContentType) const override;

    virtual bool equals(CSPDirective aDirective) const override;

  private:
    bool mRestrictWorkers;
};

/* =============== nsBlockAllMixedContentDirective === */

class nsBlockAllMixedContentDirective : public nsCSPDirective {
  public:
    explicit nsBlockAllMixedContentDirective(CSPDirective aDirective);
    ~nsBlockAllMixedContentDirective();

    bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                 bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const override
      { return false; }

    bool permits(nsIURI* aUri) const
      { return false; }

    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                bool aParserCreated) const override
      { return false; }

    void toString(nsAString& outStr) const override;

    void addSrcs(const nsTArray<nsCSPBaseSrc*>& aSrcs) override
      {  MOZ_ASSERT(false, "block-all-mixed-content does not hold any srcs"); }

    void getDirName(nsAString& outStr) const override;
};

/* =============== nsUpgradeInsecureDirective === */

/*
 * Upgrading insecure requests includes the following actors:
 * (1) CSP:
 *     The CSP implementation whitelists the http-request
 *     in case the policy is executed in enforcement mode.
 *     The CSP implementation however does not allow http
 *     requests to succeed if executed in report-only mode.
 *     In such a case the CSP implementation reports the
 *     error back to the page.
 *
 * (2) MixedContent:
 *     The evalution of MixedContent whitelists all http
 *     requests with the promise that the http requests
 *     gets upgraded to https before any data is fetched
 *     from the network.
 *
 * (3) CORS:
 *     Does not consider the http request to be of a
 *     different origin in case the scheme is the only
 *     difference in otherwise matching URIs.
 *
 * (4) nsHttpChannel:
 *     Before connecting, the channel gets redirected
 *     to use https.
 *
 * (5) WebSocketChannel:
 *     Similar to the httpChannel, the websocketchannel
 *     gets upgraded from ws to wss.
 */
class nsUpgradeInsecureDirective : public nsCSPDirective {
  public:
    explicit nsUpgradeInsecureDirective(CSPDirective aDirective);
    ~nsUpgradeInsecureDirective();

    bool permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected,
                 bool aReportOnly, bool aUpgradeInsecure, bool aParserCreated) const override
      { return false; }

    bool permits(nsIURI* aUri) const
      { return false; }

    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                bool aParserCreated) const override
      { return false; }

    void toString(nsAString& outStr) const override;

    void addSrcs(const nsTArray<nsCSPBaseSrc*>& aSrcs) override
      {  MOZ_ASSERT(false, "upgrade-insecure-requests does not hold any srcs"); }

    void getDirName(nsAString& outStr) const override;
};

/* ===== nsRequireSRIForDirective ========================= */

class nsRequireSRIForDirective : public nsCSPDirective {
  public:
    explicit nsRequireSRIForDirective(CSPDirective aDirective);
    ~nsRequireSRIForDirective();

    void toString(nsAString& outStr) const override;

    void addType(nsContentPolicyType aType)
      { mTypes.AppendElement(aType); }
    bool hasType(nsContentPolicyType aType) const;
    bool restrictsContentType(nsContentPolicyType aType) const override;
    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce,
                bool aParserCreated) const override;
    void getDirName(nsAString& outStr) const override;

  private:
    nsTArray<nsContentPolicyType> mTypes;
};

/* =============== nsCSPPolicy ================== */

class nsCSPPolicy {
  public:
    nsCSPPolicy();
    virtual ~nsCSPPolicy();

    bool permits(CSPDirective aDirective,
                 nsIURI* aUri,
                 const nsAString& aNonce,
                 bool aWasRedirected,
                 bool aSpecific,
                 bool aParserCreated,
                 nsAString& outViolatedDirective) const;
    bool permits(CSPDirective aDir,
                 nsIURI* aUri,
                 bool aSpecific) const;
    bool allows(nsContentPolicyType aContentType,
                enum CSPKeyword aKeyword,
                const nsAString& aHashOrNonce,
                bool aParserCreated) const;
    bool allows(nsContentPolicyType aContentType,
                enum CSPKeyword aKeyword) const;
    void toString(nsAString& outStr) const;
    void toDomCSPStruct(mozilla::dom::CSP& outCSP) const;

    inline void addDirective(nsCSPDirective* aDir)
      { mDirectives.AppendElement(aDir); }

    inline void addUpgradeInsecDir(nsUpgradeInsecureDirective* aDir)
      {
        mUpgradeInsecDir = aDir;
        addDirective(aDir);
      }

    bool hasDirective(CSPDirective aDir) const;

    inline void setReportOnlyFlag(bool aFlag)
      { mReportOnly = aFlag; }

    inline bool getReportOnlyFlag() const
      { return mReportOnly; }

    void getReportURIs(nsTArray<nsString> &outReportURIs) const;

    void getDirectiveStringForContentType(nsContentPolicyType aContentType,
                                          nsAString& outDirective) const;

    void getDirectiveAsString(CSPDirective aDir, nsAString& outDirective) const;

    uint32_t getSandboxFlags() const;

    bool requireSRIForType(nsContentPolicyType aContentType);

    inline uint32_t getNumDirectives() const
      { return mDirectives.Length(); }

    bool visitDirectiveSrcs(CSPDirective aDir, nsCSPSrcVisitor* aVisitor) const;

  private:
    nsUpgradeInsecureDirective* mUpgradeInsecDir;
    nsTArray<nsCSPDirective*>   mDirectives;
    bool                        mReportOnly;
};

#endif /* nsCSPUtils_h___ */
