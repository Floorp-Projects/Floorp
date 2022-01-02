// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// <subdivisionAlias type="frg" replacement="frges" reason="deprecated"/>
assertEq(Intl.getCanonicalLocales("fr-u-rg-frg")[0], "fr-u-rg-frges");
assertEq(Intl.getCanonicalLocales("fr-u-sd-frg")[0], "fr-u-sd-frges");

// <subdivisionAlias type="frgf" replacement="GF" reason="overlong"/>
assertEq(Intl.getCanonicalLocales("fr-u-rg-frgf")[0], "fr-u-rg-gfzzzz");
assertEq(Intl.getCanonicalLocales("fr-u-sd-frgf")[0], "fr-u-sd-gfzzzz");

if (typeof reportCompare === "function")
  reportCompare(true, true);
