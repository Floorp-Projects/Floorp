// |reftest| skip-if(!this.hasOwnProperty('Intl'))

if (typeof getDefaultLocale === "undefined") {
    var getDefaultLocale = SpecialPowers.Cu.getJSTestingFunctions().getDefaultLocale;
}
if (typeof setDefaultLocale === "undefined") {
    var setDefaultLocale = SpecialPowers.Cu.getJSTestingFunctions().setDefaultLocale;
}

let defaultLocale = null;

function withLocale(locale, fn) {
    if (defaultLocale === null)
        defaultLocale = getDefaultLocale();

    setDefaultLocale(locale);
    try {
        fn();
    } finally {
        setDefaultLocale(defaultLocale);
    }
}

// This test assumes Azerbaijani ("az") is a supported locale.
const supported = Intl.Collator.supportedLocalesOf("az");
assertEq(supported.length, 1);
assertEq(supported[0], "az");

withLocale("az", () => {
    // Ensure the new default locale is now active.
    assertEq(new Intl.Collator().resolvedOptions().locale, "az");

    // "az" is the active default locale, so explicitly requesting "az" should succeed.
    assertEq(new Intl.Collator("az").resolvedOptions().locale, "az");

    // ICU doesn't provide a specialised "az-Cyrl" locale, so we fallback to "az".
    assertEq(new Intl.Collator("az-Cyrl").resolvedOptions().locale, "az");

    // ICU doesn't provide a specialised "az-Cyrl-AZ" locale, so we fallback to "az".
    assertEq(new Intl.Collator("az-Cyrl-AZ").resolvedOptions().locale, "az");
});

// As demonstrated above, "az-Cyrl-AZ" normally isn't a supported Intl.Collator locale. But when
// used as the default locale, it gets promoted to being supported, because its parent locale "az"
// is supported and can act as a fallback.
//
// This works as follows:
// We accept any default locale as long as it can be supported either explicitly or implicitly
// through a fallback. But when we claim a default locale is supported, we also need to make sure
// we report any parent locale as being supported. So when "az-Cyrl-AZ" is accepted as the
// default locale, we also need to report its parent locale "az-Cyrl" as a supported locale.
//
// The reason we're doing this, is to make sure we aren't limiting the supported default locale to
// the intersection of the sets of supported locales for each Intl service constructor. Also see
// the requirements in <https://tc39.es/ecma402/#sec-internal-slots>, which state that the default
// locale must be a member of [[AvailableLocales]] for every Intl service constructor.
//
// So the following statement must hold:
//
// ∀ Constructor ∈ IntlConstructors: DefaultLocale ∈ Constructor.[[AvailableLocales]]
//
// This can trivially be achieved when we restrict the default locale to:
//
//                 { RequestedLocale            if RequestedLocale ∈ (∩ C.[[AvailableLocales]])
//                 {                                                  C ∈ IntlConstructors
//                 {
// DefaultLocale = { Fallback(RequestedLocale)  if Fallback(RequestedLocale) ∈ (∩ C.[[AvailableLocales]])
//                 {                                                            C ∈ IntlConstructors
//                 {
//                 { LastDitchLocale            otherwise
//
// But that severely restricts the possible default locales. For example, "az-Cyrl-AZ" is supported
// by all Intl constructors except Intl.Collator. Intl.Collator itself only provides explicit
// support for the parent locale "az". So with the trivial solution we'd need to mark "az-Cyrl-AZ"
// as an invalid default locale and instead use its fallback locale "az".
//
// So instead of that we're using the following approach:
//
//                 { RequestedLocale    if RequestedLocale ∈ (∩ C.[[AvailableLocales]])
//                 {                                          C ∈ IntlConstructors
//                 {
// DefaultLocale = { RequestedLocale    if Fallback(RequestedLocale) ∈ (∩ C.[[AvailableLocales]])
//                 {                                                    C ∈ IntlConstructors
//                 {
//                 { LastDitchLocale    otherwise
//
// So even when the requested default locale is only implicitly supported through a fallback, we
// still accept it as a valid default locale.
withLocale("az-Cyrl-AZ", () => {
    // Ensure the new default locale is now active.
    assertEq(new Intl.Collator().resolvedOptions().locale, "az-Cyrl-AZ");

    // "az-Cyrl-AZ" is the active default locale, so explicitly requesting the parent locale
    // "az" should succeed.
    assertEq(new Intl.Collator("az").resolvedOptions().locale, "az");

    // "az-Cyrl-AZ" is the active default locale, so explicitly requesting the parent locale
    // "az-Cyrl" should succeed.
    assertEq(new Intl.Collator("az-Cyrl").resolvedOptions().locale, "az-Cyrl");

    // "az-Cyrl-AZ" is the active default locale, so explicitly requesting "az-Cyrl-AZ"
    // should succeed.
    assertEq(new Intl.Collator("az-Cyrl-AZ").resolvedOptions().locale, "az-Cyrl-AZ");
});

if (typeof reportCompare === "function")
    reportCompare(0, 0);
