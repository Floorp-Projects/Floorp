// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const tests = [
  // Special values: Zeros and non-finite values.
  {
    value: 0,
    options: {},
    roundingModes: {
      ceil: "0",
      floor: "0",
      expand: "0",
      trunc: "0",
      halfCeil: "0",
      halfFloor: "0",
      halfExpand: "0",
      halfTrunc: "0",
      halfEven: "0",
    },
  },
  {
    value: -0,
    options: {},
    roundingModes: {
      ceil: "-0",
      floor: "-0",
      expand: "-0",
      trunc: "-0",
      halfCeil: "-0",
      halfFloor: "-0",
      halfExpand: "-0",
      halfTrunc: "-0",
      halfEven: "-0",
    },
  },
  {
    value: -Infinity,
    options: {},
    roundingModes: {
      ceil: "-∞",
      floor: "-∞",
      expand: "-∞",
      trunc: "-∞",
      halfCeil: "-∞",
      halfFloor: "-∞",
      halfExpand: "-∞",
      halfTrunc: "-∞",
      halfEven: "-∞",
    },
  },
  {
    value: Infinity,
    options: {},
    roundingModes: {
      ceil: "∞",
      floor: "∞",
      expand: "∞",
      trunc: "∞",
      halfCeil: "∞",
      halfFloor: "∞",
      halfExpand: "∞",
      halfTrunc: "∞",
      halfEven: "∞",
    },
  },
  {
    value: NaN,
    options: {},
    roundingModes: {
      ceil: "NaN",
      floor: "NaN",
      expand: "NaN",
      trunc: "NaN",
      halfCeil: "NaN",
      halfFloor: "NaN",
      halfExpand: "NaN",
      halfTrunc: "NaN",
      halfEven: "NaN",
    },
  },

  // Integer rounding with positive values.
  {
    value: 0.4,
    options: {maximumFractionDigits: 0},
    roundingModes: {
      ceil: "1",
      floor: "0",
      expand: "1",
      trunc: "0",
      halfCeil: "0",
      halfFloor: "0",
      halfExpand: "0",
      halfTrunc: "0",
      halfEven: "0",
    },
  },
  {
    value: 0.5,
    options: {maximumFractionDigits: 0},
    roundingModes: {
      ceil: "1",
      floor: "0",
      expand: "1",
      trunc: "0",
      halfCeil: "1",
      halfFloor: "0",
      halfExpand: "1",
      halfTrunc: "0",
      halfEven: "0",
    },
  },
  {
    value: 0.6,
    options: {maximumFractionDigits: 0},
    roundingModes: {
      ceil: "1",
      floor: "0",
      expand: "1",
      trunc: "0",
      halfCeil: "1",
      halfFloor: "1",
      halfExpand: "1",
      halfTrunc: "1",
      halfEven: "1",
    },
  },

  // Integer rounding with negative values.
  {
    value: -0.4,
    options: {maximumFractionDigits: 0},
    roundingModes: {
      ceil: "-0",
      floor: "-1",
      expand: "-1",
      trunc: "-0",
      halfCeil: "-0",
      halfFloor: "-0",
      halfExpand: "-0",
      halfTrunc: "-0",
      halfEven: "-0",
    },
  },
  {
    value: -0.5,
    options: {maximumFractionDigits: 0},
    roundingModes: {
      ceil: "-0",
      floor: "-1",
      expand: "-1",
      trunc: "-0",
      halfCeil: "-0",
      halfFloor: "-1",
      halfExpand: "-1",
      halfTrunc: "-0",
      halfEven: "-0",
    },
  },
  {
    value: -0.6,
    options: {maximumFractionDigits: 0},
    roundingModes: {
      ceil: "-0",
      floor: "-1",
      expand: "-1",
      trunc: "-0",
      halfCeil: "-1",
      halfFloor: "-1",
      halfExpand: "-1",
      halfTrunc: "-1",
      halfEven: "-1",
    },
  },

  // Fractional digits rounding with positive values.
  {
    value: 0.04,
    options: {maximumFractionDigits: 1},
    roundingModes: {
      ceil: "0.1",
      floor: "0",
      expand: "0.1",
      trunc: "0",
      halfCeil: "0",
      halfFloor: "0",
      halfExpand: "0",
      halfTrunc: "0",
      halfEven: "0",
    },
  },
  {
    value: 0.05,
    options: {maximumFractionDigits: 1},
    roundingModes: {
      ceil: "0.1",
      floor: "0",
      expand: "0.1",
      trunc: "0",
      halfCeil: "0.1",
      halfFloor: "0",
      halfExpand: "0.1",
      halfTrunc: "0",
      halfEven: "0",
    },
  },
  {
    value: 0.06,
    options: {maximumFractionDigits: 1},
    roundingModes: {
      ceil: "0.1",
      floor: "0",
      expand: "0.1",
      trunc: "0",
      halfCeil: "0.1",
      halfFloor: "0.1",
      halfExpand: "0.1",
      halfTrunc: "0.1",
      halfEven: "0.1",
    },
  },

  // Fractional digits rounding with negative values.
  {
    value: -0.04,
    options: {maximumFractionDigits: 1},
    roundingModes: {
      ceil: "-0",
      floor: "-0.1",
      expand: "-0.1",
      trunc: "-0",
      halfCeil: "-0",
      halfFloor: "-0",
      halfExpand: "-0",
      halfTrunc: "-0",
      halfEven: "-0",
    },
  },
  {
    value: -0.05,
    options: {maximumFractionDigits: 1},
    roundingModes: {
      ceil: "-0",
      floor: "-0.1",
      expand: "-0.1",
      trunc: "-0",
      halfCeil: "-0",
      halfFloor: "-0.1",
      halfExpand: "-0.1",
      halfTrunc: "-0",
      halfEven: "-0",
    },
  },
  {
    value: -0.06,
    options: {maximumFractionDigits: 1},
    roundingModes: {
      ceil: "-0",
      floor: "-0.1",
      expand: "-0.1",
      trunc: "-0",
      halfCeil: "-0.1",
      halfFloor: "-0.1",
      halfExpand: "-0.1",
      halfTrunc: "-0.1",
      halfEven: "-0.1",
    },
  },
];

for (let {value, options, roundingModes} of tests) {
  for (let [roundingMode, expected] of Object.entries(roundingModes)) {
    let nf = new Intl.NumberFormat("en", {...options, roundingMode});
    assertEq(nf.format(value), expected, `value=${value}, roundingMode=${roundingMode}`);
    assertEq(nf.resolvedOptions().roundingMode, roundingMode);
  }
}

// Default value is "halfExpand".
assertEq(new Intl.NumberFormat().resolvedOptions().roundingMode, "halfExpand");

// Invalid values.
for (let roundingMode of ["", null, "halfOdd", "halfUp", "Up", "up"]){
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", {roundingMode}), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
