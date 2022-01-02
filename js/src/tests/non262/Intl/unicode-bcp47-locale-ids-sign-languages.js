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
// CLDR 38 added these mappings to CLDR, too.

assertEq(Intl.getCanonicalLocales("sgn-DE")[0], "gsg");
assertEq(Intl.getCanonicalLocales("sgn-DD")[0], "gsg");

assertEq(Intl.getCanonicalLocales("sgn-276")[0], "gsg");
assertEq(Intl.getCanonicalLocales("sgn-278")[0], "gsg");
assertEq(Intl.getCanonicalLocales("sgn-280")[0], "gsg");

if (typeof reportCompare === "function")
    reportCompare(true, true);
