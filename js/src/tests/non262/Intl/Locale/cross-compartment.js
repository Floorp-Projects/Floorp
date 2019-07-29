// |reftest| skip-if(!this.hasOwnProperty('Intl')||(!this.Intl.Locale&&!this.hasOwnProperty('addIntlExtras')))

var g = newGlobal();
addIntlLocale(g);

var tag = "de-Latn-AT-u-ca-gregory-nu-latn-co-phonebk-kf-false-kn-hc-h23";
var locale = new Intl.Locale(tag);
var ccwLocale = new g.Intl.Locale(tag);

for (var [key, {get, value = get}] of Object.entries(Object.getOwnPropertyDescriptors(Intl.Locale.prototype))) {
    if (typeof value === "function") {
        if (key !== "constructor") {
            var expectedValue = value.call(locale);

            if (typeof expectedValue === "string" || typeof expectedValue === "boolean") {
                assertEq(value.call(ccwLocale), expectedValue, key);
            } else if (expectedValue instanceof Intl.Locale) {
                assertEq(value.call(ccwLocale).toString(), expectedValue.toString(), key);
            } else {
                throw new Error("unexpected result value");
            }
        } else {
            assertEq(new value(ccwLocale).toString(), new value(locale).toString(), key);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
