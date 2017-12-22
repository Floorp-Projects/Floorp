// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Tests the PluralRules.resolvedOptions function for overriden Array[Symbol.species].

var pl = new Intl.PluralRules("de");

Object.defineProperty(Array, Symbol.species, {
    value: function() {
        return new Proxy(["?"], {
            get(t, pk, r) {
                return Reflect.get(t, pk, r);
            },
            defineProperty(t, pk) {
                return true;
            }
        });
    }
});

var pluralCategories = pl.resolvedOptions().pluralCategories;

assertEqArray(pluralCategories, ["one", "other"]);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
