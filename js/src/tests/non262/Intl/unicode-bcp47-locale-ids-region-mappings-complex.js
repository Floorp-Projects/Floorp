// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// CLDR contains region mappings where the replacement region depends on the
// likely subtags from the language and script subtags.
//
// For example, the breakup of the Soviet Union ("SU") means that the region of
// the Soviet Union ("SU") is replaced by Russia ("RU"), Armenia ("AM"), or
// many others -- depending on the specified (or merely likely) language and
// script subtags:
//
// <territoryAlias type="SU" replacement="RU AM AZ BY EE GE KZ KG LV LT MD TJ TM UA UZ" reason="deprecated"/>
//
// Armenia can be the preferred region when the language is "hy" (Armenian) or
// the script is "Armn" (Armenian).
//
// <likelySubtag from="hy" to="hy_Armn_AM"/>
// <likelySubtag from="und_Armn" to="hy_Armn_AM"/>
assertEq(Intl.getCanonicalLocales("ru-SU")[0], "ru-RU");
assertEq(Intl.getCanonicalLocales("en-SU")[0], "en-RU");
assertEq(Intl.getCanonicalLocales("und-SU")[0], "und-RU");
assertEq(Intl.getCanonicalLocales("und-Latn-SU")[0], "und-Latn-RU");
assertEq(Intl.getCanonicalLocales("hy-SU")[0], "hy-AM");
assertEq(Intl.getCanonicalLocales("und-Armn-SU")[0], "und-Armn-AM");

// <territoryAlias type="CS" replacement="RS ME" reason="deprecated"/>
//
// The following likely-subtags entries contain "RS" and "ME":
//
// <likelySubtag from="sr" to="sr_Cyrl_RS"/>
// <likelySubtag from="sr_ME" to="sr_Latn_ME"/>
// <likelySubtag from="und_RS" to="sr_Cyrl_RS"/>
// <likelySubtag from="und_ME" to="sr_Latn_ME"/>
//
// In this case there is no language/script combination (without a region
// subtag) where "ME" is ever chosen, so the replacement is always "RS".
assertEq(Intl.getCanonicalLocales("sr-CS")[0], "sr-RS");
assertEq(Intl.getCanonicalLocales("sr-Latn-CS")[0], "sr-Latn-RS");
assertEq(Intl.getCanonicalLocales("sr-Cyrl-CS")[0], "sr-Cyrl-RS");

// The existing region in the source locale identifier is ignored when selecting
// the likely replacement region. For example take "az-NT", which is Azerbaijani
// spoken in the Neutral Zone. The replacement region for "NT" is either
// "SA" (Saudi-Arabia) or "IQ" (Iraq), and there is also a likely subtags entry
// for "az-IQ". But when only looking at the language subtag in "az-NT", "az" is
// always resolved to "az-Latn-AZ", and because "AZ" is not in the list ["SA",
// "IQ"], the final replacement region is the default for "NT", namely "SA".
// That means "az-NT" will be canonicalised to "az-SA" and not "az-IQ", even
// though the latter may be a more sensible candidate based on the actual usage
// of the target locales.
//
// <territoryAlias type="NT" replacement="SA IQ" reason="deprecated"/>
// <likelySubtag from="az_IQ" to="az_Arab_IQ"/>
// <likelySubtag from="az" to="az_Latn_AZ"/>
assertEq(Intl.getCanonicalLocales("az-NT")[0], "az-SA");

if (typeof reportCompare === "function")
    reportCompare(true, true);
