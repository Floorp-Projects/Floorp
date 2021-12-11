// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// UTS 35, 3.2.1 Canonical Unicode Locale Identifiers:
// - Any variants are in alphabetical order.

assertEq(Intl.getCanonicalLocales("en-scouse-fonipa")[0], "en-fonipa-scouse");

// Sorting in alphabetical order may turn a valid BCP 47 language tag into a
// BCP 47 language tag which is only well-formed, but no longer valid. This
// means there are potential compatibility issues when converting between
// Unicode BCP 47 locale identifiers and BCP 47 language tags.
//
// Spec: https://tools.ietf.org/html/rfc5646#section-2.2.9

// <https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry>
//
// Type: variant
// Subtag: 1994
// Description: Standardized Resian orthography
// Added: 2007-07-28
// Prefix: sl-rozaj
// Prefix: sl-rozaj-biske
// Prefix: sl-rozaj-njiva
// Prefix: sl-rozaj-osojs
// Prefix: sl-rozaj-solba
// Comments: For standardized Resian an orthography was published in 1994.

assertEq(Intl.getCanonicalLocales("sl-rozaj-biske-1994")[0], "sl-1994-biske-rozaj");

if (typeof reportCompare === "function")
    reportCompare(true, true);
