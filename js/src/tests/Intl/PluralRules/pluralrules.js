// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Tests the format function with a diverse set of locales and options.

var pr;

pr = new Intl.PluralRules("en-us");
assertEq(pr.resolvedOptions().locale, "en-US");
assertEq(pr.resolvedOptions().type, "cardinal");
assertEq(pr.resolvedOptions().pluralCategories.length, 2);

pr = new Intl.PluralRules("de", {type: 'cardinal'});
assertEq(pr.resolvedOptions().pluralCategories.length, 2);

pr = new Intl.PluralRules("de", {type: 'ordinal'});
assertEq(pr.resolvedOptions().pluralCategories.length, 1);

reportCompare(0, 0, 'ok');
