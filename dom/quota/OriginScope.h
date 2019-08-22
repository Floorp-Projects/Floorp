/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_originorpatternstring_h__
#define mozilla_dom_quota_originorpatternstring_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/Variant.h"

BEGIN_QUOTA_NAMESPACE

class OriginScope {
  class Origin {
    nsCString mOrigin;
    nsCString mOriginNoSuffix;
    UniquePtr<OriginAttributes> mAttributes;

   public:
    explicit Origin(const nsACString& aOrigin) : mOrigin(aOrigin) {
      InitMembers();
    }

    Origin(const Origin& aOther)
        : mOrigin(aOther.mOrigin),
          mOriginNoSuffix(aOther.mOriginNoSuffix),
          mAttributes(MakeUnique<OriginAttributes>(*aOther.mAttributes)) {}

    Origin(Origin&& aOther) = default;

    const nsACString& GetOrigin() const { return mOrigin; }

    void SetOrigin(const nsACString& aOrigin) {
      mOrigin = aOrigin;

      InitMembers();
    }

    const nsACString& GetOriginNoSuffix() const { return mOriginNoSuffix; }

    const OriginAttributes& GetAttributes() const {
      MOZ_ASSERT(mAttributes);

      return *mAttributes;
    }

   private:
    void InitMembers() {
      mAttributes = MakeUnique<OriginAttributes>();

      MOZ_ALWAYS_TRUE(
          mAttributes->PopulateFromOrigin(mOrigin, mOriginNoSuffix));
    }
  };

  class Prefix {
    nsCString mOriginNoSuffix;

   public:
    explicit Prefix(const nsACString& aOriginNoSuffix)
        : mOriginNoSuffix(aOriginNoSuffix) {}

    const nsCString& GetOriginNoSuffix() const { return mOriginNoSuffix; }

    void SetOriginNoSuffix(const nsACString& aOriginNoSuffix) {
      mOriginNoSuffix = aOriginNoSuffix;
    }
  };

  class Pattern {
    UniquePtr<OriginAttributesPattern> mPattern;

   public:
    explicit Pattern(const OriginAttributesPattern& aPattern)
        : mPattern(MakeUnique<OriginAttributesPattern>(aPattern)) {}

    explicit Pattern(const nsAString& aJSONPattern)
        : mPattern(MakeUnique<OriginAttributesPattern>()) {
      MOZ_ALWAYS_TRUE(mPattern->Init(aJSONPattern));
    }

    Pattern(const Pattern& aOther)
        : mPattern(MakeUnique<OriginAttributesPattern>(*aOther.mPattern)) {}

    Pattern(Pattern&& aOther) = default;

    const OriginAttributesPattern& GetPattern() const {
      MOZ_ASSERT(mPattern);

      return *mPattern;
    }

    void SetPattern(const OriginAttributesPattern& aPattern) {
      mPattern = MakeUnique<OriginAttributesPattern>(aPattern);
    }

    nsString GetJSONPattern() const {
      MOZ_ASSERT(mPattern);

      nsString result;
      MOZ_ALWAYS_TRUE(mPattern->ToJSON(result));

      return result;
    }
  };

  struct Null {};

  using DataType = Variant<Origin, Prefix, Pattern, Null>;

  DataType mData;

 public:
  OriginScope() : mData(Null()) {}

  static OriginScope FromOrigin(const nsACString& aOrigin) {
    return OriginScope(std::move(Origin(aOrigin)));
  }

  static OriginScope FromPrefix(const nsACString& aPrefix) {
    return OriginScope(std::move(Prefix(aPrefix)));
  }

  static OriginScope FromPattern(const OriginAttributesPattern& aPattern) {
    return OriginScope(std::move(Pattern(aPattern)));
  }

  static OriginScope FromJSONPattern(const nsAString& aJSONPattern) {
    return OriginScope(std::move(Pattern(aJSONPattern)));
  }

  static OriginScope FromNull() { return OriginScope(std::move(Null())); }

  bool IsOrigin() const { return mData.is<Origin>(); }

  bool IsPrefix() const { return mData.is<Prefix>(); }

  bool IsPattern() const { return mData.is<Pattern>(); }

  bool IsNull() const { return mData.is<Null>(); }

  void SetFromOrigin(const nsACString& aOrigin) {
    mData = AsVariant(Origin(aOrigin));
  }

  void SetFromPrefix(const nsACString& aPrefix) {
    mData = AsVariant(Prefix(aPrefix));
  }

  void SetFromPattern(const OriginAttributesPattern& aPattern) {
    mData = AsVariant(Pattern(aPattern));
  }

  void SetFromJSONPattern(const nsAString& aJSONPattern) {
    mData = AsVariant(Pattern(aJSONPattern));
  }

  void SetFromNull() { mData = AsVariant(Null()); }

  const nsACString& GetOrigin() const {
    MOZ_ASSERT(IsOrigin());

    return mData.as<Origin>().GetOrigin();
  }

  void SetOrigin(const nsACString& aOrigin) {
    MOZ_ASSERT(IsOrigin());

    mData.as<Origin>().SetOrigin(aOrigin);
  }

  const nsACString& GetOriginNoSuffix() const {
    MOZ_ASSERT(IsOrigin() || IsPrefix());

    if (IsOrigin()) {
      return mData.as<Origin>().GetOriginNoSuffix();
    }
    return mData.as<Prefix>().GetOriginNoSuffix();
  }

  void SetOriginNoSuffix(const nsACString& aOriginNoSuffix) {
    MOZ_ASSERT(IsPrefix());

    mData.as<Prefix>().SetOriginNoSuffix(aOriginNoSuffix);
  }

  const OriginAttributesPattern& GetPattern() const {
    MOZ_ASSERT(IsPattern());

    return mData.as<Pattern>().GetPattern();
  }

  nsString GetJSONPattern() const {
    MOZ_ASSERT(IsPattern());

    return mData.as<Pattern>().GetJSONPattern();
  }

  void SetPattern(const OriginAttributesPattern& aPattern) {
    MOZ_ASSERT(IsPattern());

    mData.as<Pattern>().SetPattern(aPattern);
  }

  bool Matches(const OriginScope& aOther) const {
    struct Matcher {
      const OriginScope& mThis;

      explicit Matcher(const OriginScope& aThis) : mThis(aThis) {}

      bool operator()(const Origin& aOther) {
        return mThis.MatchesOrigin(aOther);
      }

      bool operator()(const Prefix& aOther) {
        return mThis.MatchesPrefix(aOther);
      }

      bool operator()(const Pattern& aOther) {
        return mThis.MatchesPattern(aOther);
      }

      bool operator()(const Null& aOther) { return true; }
    };

    return aOther.mData.match(Matcher(*this));
  }

  OriginScope Clone() { return OriginScope(mData); }

  template <typename T, typename U>
  class Setter {
    typedef void (OriginScope::*Method)(const U& aOrigin);

    OriginScope* mOriginScope;
    Method mMethod;
    T mData;

   public:
    Setter(OriginScope* aOriginScope, Method aMethod)
        : mOriginScope(aOriginScope), mMethod(aMethod) {}

    Setter(Setter&& aOther)
        : mOriginScope(aOther.mOriginScope),
          mMethod(aOther.mMethod),
          mData(std::move(aOther.mData)) {}

    ~Setter() { ((*mOriginScope).*mMethod)(mData); }

    operator T&() { return mData; }

    operator T*() { return &mData; }

   private:
    Setter() = delete;
    Setter(const Setter&) = delete;
    Setter& operator=(const Setter&) = delete;
    Setter& operator=(const Setter&&) = delete;
  };

  /**
   * Magic helper for cases where you are calling a method that takes a
   * ns(A)CString outparam to write an origin into. This method returns a helper
   * temporary that returns the underlying string storage, then on the
   * destruction of the temporary at the conclusion of the call, automatically
   * invokes SetFromOrigin using the origin value that was written into the
   * string.
   */
  Setter<nsCString, nsACString> AsOriginSetter() {
    return Setter<nsCString, nsACString>(this, &OriginScope::SetFromOrigin);
  }

 private:
  // Move constructors
  explicit OriginScope(const Origin&& aOrigin) : mData(aOrigin) {}

  explicit OriginScope(const Prefix&& aPrefix) : mData(aPrefix) {}

  explicit OriginScope(const Pattern&& aPattern) : mData(aPattern) {}

  explicit OriginScope(const Null&& aNull) : mData(aNull) {}

  // Copy constructor
  explicit OriginScope(const DataType& aOther) : mData(aOther) {}

  bool MatchesOrigin(const Origin& aOther) const {
    struct OriginMatcher {
      const Origin& mOther;

      explicit OriginMatcher(const Origin& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return aThis.GetOrigin().Equals(mOther.GetOrigin());
      }

      bool operator()(const Prefix& aThis) {
        return aThis.GetOriginNoSuffix().Equals(mOther.GetOriginNoSuffix());
      }

      bool operator()(const Pattern& aThis) {
        return aThis.GetPattern().Matches(mOther.GetAttributes());
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(OriginMatcher(aOther));
  }

  bool MatchesPrefix(const Prefix& aOther) const {
    struct PrefixMatcher {
      const Prefix& mOther;

      explicit PrefixMatcher(const Prefix& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return aThis.GetOriginNoSuffix().Equals(mOther.GetOriginNoSuffix());
      }

      bool operator()(const Prefix& aThis) {
        return aThis.GetOriginNoSuffix().Equals(mOther.GetOriginNoSuffix());
      }

      bool operator()(const Pattern& aThis) {
        // The match will be always true here because any origin attributes
        // pattern overlaps any origin prefix (an origin prefix targets all
        // origin attributes).
        return true;
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(PrefixMatcher(aOther));
  }

  bool MatchesPattern(const Pattern& aOther) const {
    struct PatternMatcher {
      const Pattern& mOther;

      explicit PatternMatcher(const Pattern& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return mOther.GetPattern().Matches(aThis.GetAttributes());
      }

      bool operator()(const Prefix& aThis) {
        // The match will be always true here because any origin attributes
        // pattern overlaps any origin prefix (an origin prefix targets all
        // origin attributes).
        return true;
      }

      bool operator()(const Pattern& aThis) {
        return aThis.GetPattern().Overlaps(mOther.GetPattern());
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    PatternMatcher patternMatcher(aOther);
    return mData.match(PatternMatcher(aOther));
  }

  bool operator==(const OriginScope& aOther) = delete;
};

END_QUOTA_NAMESPACE

#endif  // mozilla_dom_quota_originorpatternstring_h__
