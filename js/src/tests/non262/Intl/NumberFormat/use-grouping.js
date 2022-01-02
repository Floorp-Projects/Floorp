// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const tests = {
  // minimumGroupingDigits is one for "en" (English).
  "en": {
    values: [
      {
        value: 1000,
        useGroupings: {
          auto: "1,000",
          always: "1,000",
          min2: "1000",
          "": "1000",
        },
      },
      {
        value: 10000,
        useGroupings: {
          auto: "10,000",
          always: "10,000",
          min2: "10,000",
          "": "10000",
        },
      },
    ],
  },

  // minimumGroupingDigits is two for "pl" (Polish).
  "pl": {
    values: [
      {
        value: 1000,
        useGroupings: {
          auto: "1000",
          always: "1 000",
          min2: "1000",
          "": "1000",
        },
      },
      {
        value: 10000,
        useGroupings: {
          auto: "10 000",
          always: "10 000",
          min2: "10 000",
          "": "10000",
        },
      },
    ],
  },
};

for (let [locale, {options = {}, values}] of Object.entries(tests)) {
  for (let {value, useGroupings} of values) {
    for (let [useGrouping, expected] of Object.entries(useGroupings)) {
      let nf = new Intl.NumberFormat(locale, {...options, useGrouping});
      assertEq(nf.format(value), expected, `locale=${locale}, value=${value}, useGrouping=${useGrouping}`);
    }
  }
}

// Resolved options.
for (let [useGrouping, expected] of [
  [false, false],
  ["", false],
  [0, false],
  [null, false],

  ["auto", "auto"],
  [undefined, "auto"],

  ["always", "always"],
  [true, "always"],

  ["min2", "min2"],
]) {
  let nf = new Intl.NumberFormat("en", {useGrouping});
  assertEq(nf.resolvedOptions().useGrouping , expected);
}

// Invalid values.
for (let useGrouping of ["true", "false", "none", "yes", "no"]){
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", {useGrouping}), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
