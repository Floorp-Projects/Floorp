// |reftest| skip-if(!this.hasOwnProperty("Intl"))

var otherGlobal = newGlobal();

var pluralRules = new Intl.PluralRules();
var ccwPluralRules = new otherGlobal.Intl.PluralRules();

// Test Intl.PluralRules.prototype.select with a CCW object.
var Intl_PluralRules_select = Intl.PluralRules.prototype.select;

assertEq(Intl_PluralRules_select.call(ccwPluralRules, 0),
         Intl_PluralRules_select.call(pluralRules, 0));

// Test Intl.PluralRules.prototype.resolvedOptions with a CCW object.
var Intl_PluralRules_resolvedOptions = Intl.PluralRules.prototype.resolvedOptions;

assertEq(deepEqual(Intl_PluralRules_resolvedOptions.call(ccwPluralRules),
                   Intl_PluralRules_resolvedOptions.call(pluralRules)),
         true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
