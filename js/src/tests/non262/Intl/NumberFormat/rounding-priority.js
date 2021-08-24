// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const tests = [
  // Rounding conflict with maximum fraction/significand digits.
  {
    value: 4.321,
    options: {
      maximumFractionDigits: 2,
      maximumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "4.3",
      lessPrecision: "4.3",
      morePrecision: "4.32",
    },
  },
  {
    value: 4.321,
    options: {
      maximumFractionDigits: 2,
      minimumFractionDigits: 2,
      maximumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "4.3",
      lessPrecision: "4.30",
      morePrecision: "4.32",
    },
  },
  {
    value: 4.321,
    options: {
      maximumFractionDigits: 2,
      maximumSignificantDigits: 2,
      minimumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "4.3",
      lessPrecision: "4.3",
      morePrecision: "4.32",
    },
  },
  {
    value: 4.321,
    options: {
      maximumFractionDigits: 2,
      minimumFractionDigits: 2,
      maximumSignificantDigits: 2,
      minimumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "4.3",
      lessPrecision: "4.30",
      morePrecision: "4.32",
    },
  },

  // Rounding conflict with minimum fraction/significand digits.
  {
    value: 1.0,
    options: {
      minimumFractionDigits: 2,
      minimumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "1.0",
      // Returning "1.00" for both options seems unexpected. Also filed at
      // <https://github.com/tc39/proposal-intl-numberformat-v3/issues/52>.
      lessPrecision: "1.00",
      morePrecision: "1.00",
    },
  },
  {
    value: 1.0,
    options: {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
      minimumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "1.0",
      lessPrecision: "1.00",
      morePrecision: "1.00",
    },
  },
  {
    value: 1.0,
    options: {
      minimumFractionDigits: 2,
      minimumSignificantDigits: 2,
      maximumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "1.0",
      lessPrecision: "1.00",
      morePrecision: "1.00",
    },
  },
  {
    value: 1.0,
    options: {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
      minimumSignificantDigits: 2,
      maximumSignificantDigits: 2,
    },
    roundingPriorities: {
      auto: "1.0",
      lessPrecision: "1.00",
      morePrecision: "1.00",
    },
  },
];

for (let {value, options, roundingPriorities} of tests) {
  for (let [roundingPriority, expected] of Object.entries(roundingPriorities)) {
    let nf = new Intl.NumberFormat("en", {...options, roundingPriority});
    assertEq(nf.resolvedOptions().roundingPriority, roundingPriority);
    assertEq(nf.format(value), expected, `value=${value}, roundingPriority=${roundingPriority}`);
  }
}

// Default value of "auto".
assertEq(new Intl.NumberFormat().resolvedOptions().roundingPriority, "auto");

// Invalid values.
for (let roundingPriority of ["", null, "more", "less", "never"]){
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", {roundingPriority}), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
