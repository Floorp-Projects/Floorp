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

// This test assumes German ("de") is a supported locale.
const supported = Intl.Collator.supportedLocalesOf("de");
assertEq(supported.length, 1);
assertEq(supported[0], "de");

withLocale("de", () => {
    // Ensure the new default locale is now active.
    assertEq(new Intl.Collator().resolvedOptions().locale, "de");

    // "de" is the active default locale, so explicitly requesting "de" should succeed.
    assertEq(new Intl.Collator("de").resolvedOptions().locale, "de");

    // ICU doesn't provide a specialised "de-ZA" locale, so we fallback to "de".
    assertEq(new Intl.Collator("de-ZA").resolvedOptions().locale, "de");

    // ICU doesn't provide a specialised "de-ZA" locale, so we fallback to "de".
    assertEq(new Intl.Collator("de-ZA-x-private").resolvedOptions().locale, "de");
});

// As demonstrated above, "de-ZA-x-private" normally isn't a supported Intl.Collator locale. But
// when used as the default locale, it gets promoted to being supported, because its parent locale
// "de" is supported and can act as a fallback.
//
// This works as follows:
// We accept any default locale as long as it can be supported either explicitly or implicitly
// through a fallback. But when we claim a default locale is supported, we also need to make sure
// we report any parent locale as being supported. So when "de-ZA-x-private" is accepted as the
// default locale, we also need to report its parent locale "de-ZA" as a supported locale.
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
// But that severely restricts the possible default locales. For example, "de-CH" is supported by
// all Intl constructors except Intl.Collator. Intl.Collator itself only provides explicit support
// for the parent locale "de". So with the trivial solution we'd need to mark "de-CH" as an invalid
// default locale and instead use its fallback locale "de".
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
withLocale("de-ZA-x-private", () => {
    // Ensure the new default locale is now active.
    assertEq(new Intl.Collator().resolvedOptions().locale, "de-ZA-x-private");

    // "de-ZA-x-private" is the active default locale, so explicitly requesting the parent locale
    // "de" should succeed.
    assertEq(new Intl.Collator("de").resolvedOptions().locale, "de");

    // "de-ZA-x-private" is the active default locale, so explicitly requesting the parent locale
    // "de-ZA" should succeed.
    assertEq(new Intl.Collator("de-ZA").resolvedOptions().locale, "de-ZA");

    // "de-ZA-x-private" is the active default locale, so explicitly requesting "de-ZA-x-private"
    // should succeed.
    assertEq(new Intl.Collator("de-ZA-x-private").resolvedOptions().locale, "de-ZA-x-private");
});

if (typeof reportCompare === "function")
    reportCompare(0, 0);
