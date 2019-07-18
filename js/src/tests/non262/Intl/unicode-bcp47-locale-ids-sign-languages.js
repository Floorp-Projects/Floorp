// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// The IANA language subtag registry contains replacements for sign language
// codes marked as redundant. For example:
//
//   Type: redundant
//   Tag: sgn-DE
//   Description: German Sign Language
//   Added: 2001-11-11
//   Deprecated: 2009-07-29
//   Preferred-Value: gsg
//
// CLDR doesn't contain these mappings. Make sure we follow CLDR instead of IANA.

assertEq(Intl.getCanonicalLocales("sgn-DE")[0], "sgn-DE");

if (typeof reportCompare === "function")
    reportCompare(true, true);
