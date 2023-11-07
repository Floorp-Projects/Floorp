// |reftest| skip-if(!this.hasOwnProperty('Intl'))

const tests = {
  "en": {
    long: {
      "USD": "US Dollar",
      "EUR": "Euro",
      "FRF": "French Franc",
      "CNY": "Chinese Yuan",
      "XAU": "Gold",
    },
    short: {
      "USD": "$",
      "EUR": "€",
      "FRF": "FRF",
      "CNY": "CN¥",
      "XAU": "XAU",
    },
    narrow: {
      "CNY": "¥",
    },
  },
  "de": {
    long: {
      "USD": "US-Dollar",
      "EUR": "Euro",
      "FRF": "Französischer Franc",
      "CNY": "Renminbi Yuan",
      "XAU": "Unze Gold",
    },
    short: {
      "USD": "$",
      "EUR": "€",
      "FRF": "FRF",
      "CNY": "CN¥",
      "XAU": "XAU",
    },
    narrow: {
      "CNY": "¥",
    },
  },
  "fr": {
    long: {
      "USD": "dollar des États-Unis",
      "EUR": "euro",
      "FRF": "franc français",
      "CNY": "yuan renminbi chinois",
      "XAU": "or",
    },
    short: {
      "USD": "$US",
      "EUR": "€",
      "FRF": "F",
      "CNY": "CNY",
      "XAU": "XAU",
    },
    narrow: {
      "USD": "$",
      "CNY": "¥",
    },
  },
  "zh": {
    long: {
      "USD": "美元",
      "EUR": "欧元",
      "FRF": "法国法郎",
      "CNY": "人民币",
      "XAU": "黄金",
    },
    short: {
      "USD": "US$",
      "EUR": "€",
      "FRF": "FRF",
      "CNY": "¥",
      "XAU": "XAU",
    },
    narrow: {
      "USD": "$",
    },
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "currency", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "currency");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [currency, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(currency), expected);

      // Also works with objects.
      assertEq(dn.of(Object(currency)), expected);
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "currency"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(null), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);

  // Throws an error if |code| isn't a well-formed currency code.
  assertThrowsInstanceOf(() => dn.of("us"), RangeError);
  assertThrowsInstanceOf(() => dn.of("euro"), RangeError);
  assertThrowsInstanceOf(() => dn.of("€uro"), RangeError);
}

// Test fallback behaviour.
{
  let dn1 = new Intl.DisplayNames("en", {type: "currency"});
  let dn2 = new Intl.DisplayNames("en", {type: "currency", fallback: "code"});
  let dn3 = new Intl.DisplayNames("en", {type: "currency", fallback: "none"});

  assertEq(dn1.resolvedOptions().fallback, "code");
  assertEq(dn2.resolvedOptions().fallback, "code");
  assertEq(dn3.resolvedOptions().fallback, "none");

  // "AAA" is not a registered currency code.
  assertEq(dn1.of("AAA"), "AAA");
  assertEq(dn2.of("AAA"), "AAA");
  assertEq(dn3.of("AAA"), undefined);

  // The returned fallback is in canonical case.
  assertEq(dn1.of("aaa"), "AAA");
  assertEq(dn2.of("aaa"), "AAA");
  assertEq(dn3.of("aaa"), undefined);
}

// Test when case isn't canonical.
{
  let dn = new Intl.DisplayNames("en", {type: "currency", fallback: "none"});

  assertEq(dn.of("USD"), "US Dollar");
  assertEq(dn.of("usd"), "US Dollar");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
