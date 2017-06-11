// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Tests the getCanonicalLocales function for overriden Array[Symbol.species].

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

var arr = Intl.getCanonicalLocales("de-x-private");

assertEqArray(arr, ["de-x-private"]);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
