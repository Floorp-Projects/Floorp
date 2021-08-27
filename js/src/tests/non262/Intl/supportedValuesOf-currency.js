// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const currencies = Intl.supportedValuesOf("currency");

assertEq(new Set(currencies).size, currencies.length, "No duplicates are present");
assertEqArray(currencies, [...currencies].sort(), "The list is sorted");

const codeRE = /^[A-Z]{3}$/;
for (let currency of currencies) {
  assertEq(codeRE.test(currency), true, `${currency} is a 3-letter ISO 4217 currency code`);
}

for (let currency of currencies) {
  let obj = new Intl.NumberFormat("en", {style: "currency", currency});
  assertEq(obj.resolvedOptions().currency, currency, `${currency} is supported by NumberFormat`);
}

for (let currency of currencies) {
  let obj = new Intl.DisplayNames("en", {type: "currency", fallback: "none"});
  assertEq(typeof obj.of(currency), "string", `${currency} is supported by DisplayNames`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
