#ifndef mozilla_intl_Locale_h__
#define mozilla_intl_Locale_h__

#include "nsString.h"

namespace mozilla {
namespace intl {

/**
 * Locale object, a BCP47-style tag decomposed into subtags for
 * matching purposes.
 *
 * If constructed with aRange = true, any missing subtags will be
 * set to "*".
 */
class Locale {
  public:
    Locale(const nsCString& aLocale, bool aRange);

    bool Matches(const Locale& aLocale) const;
    bool LanguageMatches(const Locale& aLocale) const;


    void SetVariantRange();
    void SetRegionRange();

    // returns false if nothing changed
    bool AddLikelySubtags();
    bool AddLikelySubtagsWithoutRegion();

    const nsCString& AsString() const {
      return mLocaleStr;
    }

    bool operator== (const Locale& aOther) {
      const auto& cmp = nsCaseInsensitiveCStringComparator();
      return mLanguage.Equals(aOther.mLanguage, cmp) &&
             mScript.Equals(aOther.mScript, cmp) &&
             mRegion.Equals(aOther.mRegion, cmp) &&
             mVariant.Equals(aOther.mVariant, cmp);
    }

  private:
    const nsCString& mLocaleStr;
    nsCString mLanguage;
    nsCString mScript;
    nsCString mRegion;
    nsCString mVariant;

    bool AddLikelySubtagsForLocale(const nsACString& aLocale);
};

} // intl
} // namespace mozilla

#endif /* mozilla_intl_Locale_h__ */
