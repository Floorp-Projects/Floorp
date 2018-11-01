// |reftest| skip-if(!this.hasOwnProperty("Intl"))

var otherGlobal = newGlobal();

var collator = new Intl.Collator();
var ccwCollator = new otherGlobal.Intl.Collator();

// Test Intl.Collator.prototype.compare with a CCW object.
var Intl_Collator_compare_get = Object.getOwnPropertyDescriptor(Intl.Collator.prototype, "compare").get;

assertEq(Intl_Collator_compare_get.call(ccwCollator)("a", "A"),
         Intl_Collator_compare_get.call(collator)("a", "A"));

// Test Intl.Collator.prototype.resolvedOptions with a CCW object.
var Intl_Collator_resolvedOptions = Intl.Collator.prototype.resolvedOptions;

assertEq(deepEqual(Intl_Collator_resolvedOptions.call(ccwCollator),
                   Intl_Collator_resolvedOptions.call(collator)),
         true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
