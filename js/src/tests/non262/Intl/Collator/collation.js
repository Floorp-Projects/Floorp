// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Collation can be set as Unicode locale extension or as a property.
{
  let c1 = new Intl.Collator("de", {usage: "sort"});
  let c2 = new Intl.Collator("de", {usage: "sort", collation: "phonebk"});
  let c3 = new Intl.Collator("de-u-co-phonebk", {usage: "sort"});

  assertEq(c1.resolvedOptions().locale, "de");
  assertEq(c2.resolvedOptions().locale, "de");
  assertEq(c3.resolvedOptions().locale, "de-u-co-phonebk");

  assertEq(c1.resolvedOptions().collation, "default");
  assertEq(c2.resolvedOptions().collation, "phonebk");
  assertEq(c3.resolvedOptions().collation, "phonebk");

  assertEq(c1.compare("ä", "ae"), -1);
  assertEq(c2.compare("ä", "ae"), 1);
  assertEq(c3.compare("ä", "ae"), 1);
}

// Collation property overrides any Unicode locale extension.
{
  let c1 = new Intl.Collator("de-u-co-eor", {usage: "sort"});
  let c2 = new Intl.Collator("de-u-co-eor", {usage: "sort", collation: "phonebk"});

  // Ensure "eor" collation is supported.
  assertEq(c1.resolvedOptions().locale, "de-u-co-eor");
  assertEq(c1.resolvedOptions().collation, "eor");

  // "phonebk" property overrides the Unicode locale extension.
  assertEq(c2.resolvedOptions().locale, "de");
  assertEq(c2.resolvedOptions().collation, "phonebk");

  assertEq(c1.compare("ä", "ae"), -1);
  assertEq(c2.compare("ä", "ae"), 1);
}

// The default sort collation can't be requested.
{
  // The default sort collation for Swedish (sv) was "reformed" before CLDR 42.
  // It wasn't possible to override this and select the default root sort
  // collation. Use English (en) as a locale which uses the root sort collation
  // for comparison.
  let c1 = new Intl.Collator("sv", {usage: "sort"});
  let c2 = new Intl.Collator("sv-u-co-reformed", {usage: "sort"});
  let c3 = new Intl.Collator("sv-u-co-standard", {usage: "sort"});
  let c4 = new Intl.Collator("sv-u-co-default", {usage: "sort"});
  let c5 = new Intl.Collator("en", {usage: "sort"});

  assertEq(c1.resolvedOptions().locale, "sv");
  assertEq(c2.resolvedOptions().locale, "sv");
  assertEq(c3.resolvedOptions().locale, "sv");
  assertEq(c4.resolvedOptions().locale, "sv");
  assertEq(c5.resolvedOptions().locale, "en");

  assertEq(c1.resolvedOptions().collation, "default");
  assertEq(c2.resolvedOptions().collation, "default");
  assertEq(c3.resolvedOptions().collation, "default");
  assertEq(c4.resolvedOptions().collation, "default");
  assertEq(c5.resolvedOptions().collation, "default");

  assertEq(c1.compare("y", "ü"), -1);
  assertEq(c2.compare("y", "ü"), -1);
  assertEq(c3.compare("y", "ü"), -1);
  assertEq(c4.compare("y", "ü"), -1);
  assertEq(c5.compare("y", "ü"), 1);
}

// Search collations ignore any collation overrides.
{
  let c1 = new Intl.Collator("de", {usage: "search"});
  let c2 = new Intl.Collator("de", {usage: "search", collation: "phonebk"});
  let c3 = new Intl.Collator("de-u-co-phonebk", {usage: "search"});

  assertEq(c1.resolvedOptions().locale, "de");
  assertEq(c2.resolvedOptions().locale, "de");
  assertEq(c3.resolvedOptions().locale, "de");

  assertEq(c1.resolvedOptions().collation, "default");
  assertEq(c2.resolvedOptions().collation, "default");
  assertEq(c3.resolvedOptions().collation, "default");

  assertEq(c1.compare("ä", "ae"), 1);
  assertEq(c2.compare("ä", "ae"), 1);
  assertEq(c3.compare("ä", "ae"), 1);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
