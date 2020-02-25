// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.wrapWithProto)

var tag = "de-Latn-AT-u-ca-gregory-nu-latn-co-phonebk-kf-false-kn-hc-h23";
var locale = new Intl.Locale(tag);
var scwLocale = wrapWithProto(locale, Intl.Locale.prototype);

for (var [key, {get, value = get}] of Object.entries(Object.getOwnPropertyDescriptors(Intl.Locale.prototype))) {
    if (typeof value === "function") {
        if (key !== "constructor") {
            var expectedValue = value.call(locale);

            if (typeof expectedValue === "string" || typeof expectedValue === "boolean") {
                assertEq(value.call(scwLocale), expectedValue, key);
            } else if (expectedValue instanceof Intl.Locale) {
                assertEq(value.call(scwLocale).toString(), expectedValue.toString(), key);
            } else {
                throw new Error("unexpected result value");
            }
        } else {
            assertEq(new value(scwLocale).toString(), new value(locale).toString(), key);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
