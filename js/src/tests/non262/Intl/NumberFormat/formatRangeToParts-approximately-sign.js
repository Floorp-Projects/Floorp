// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

if (typeof getAvailableLocalesOf === "undefined") {
  var getAvailableLocalesOf = SpecialPowers.Cu.getJSTestingFunctions().getAvailableLocalesOf;
}

const numbers = [
  0, 1, 2, 5, 10, 100, 1000, 10_000, 100_000, 1_000_000,
  0.1, 0.2, 0.5, 1.5,
  -0, -1, -2, -5,
  Infinity, -Infinity,
];

const options = {};

// List of known approximately sign in CLDR 39.
const approximatelySigns = [
  "~", "≈", "ca.", "約",
];

// Iterate over all locales and ensure we find exactly one approximately sign.
for (let locale of getAvailableLocalesOf("NumberFormat").sort()) {
  let nf = new Intl.NumberFormat(locale, options);
  for (let number of numbers) {
    let parts = nf.formatRangeToParts(number, number);
    let approx = parts.filter(part => part.type === "approximatelySign");

    assertEq(approx.length, 1);
    assertEq(approximatelySigns.some(approxSign => approx[0].value.includes(approxSign)), true);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
