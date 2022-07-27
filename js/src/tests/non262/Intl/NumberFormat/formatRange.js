// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

// String representation for Number.MAX_VALUE.
const en_Number_MAX_VALUE = "179,769,313,486,231,570" + ",000".repeat(97);
const de_Number_MAX_VALUE = en_Number_MAX_VALUE.replaceAll(",", ".");
const fr_Number_MAX_VALUE = en_Number_MAX_VALUE.replaceAll(",", " ");

const tests = {
  "en": {
    options: {},
    ranges: [
      // Values around zero.
      {start: 0, end: 0, result: "~0"},
      {start: 0, end: -0, result: "0–-0"},
      {start: -0, end: 0, result: "-0 – 0"},
      {start: -0, end: 0.1e-3, result: "-0 – 0"},
      {start: -0, end: "0.1e-3", result: "-0 – 0"},
      {start: "-0", end: 0.1e-3, result: "-0 – 0"},
      {start: -0, end: -0, result: "~-0"},
      {start: -0, end: -0.1, result: "-0 – -0.1"},

      // Values starting at negative infinity.
      {start: -Infinity, end: -Infinity, result: "~-∞"},
      {start: -Infinity, end: -0, result: "-∞ – -0"},
      {start: -Infinity, end: +0, result: "-∞ – 0"},
      {start: -Infinity, end: +Infinity, result: "-∞ – ∞"},

      // Values ending at negative infinity.
      {start: -Number.MAX_VALUE, end: -Infinity, result: "-" + en_Number_MAX_VALUE + " – -∞"},
      {start: -0, end: -Infinity, result: "-0 – -∞"},
      {start: 0, end: -Infinity, result: "0–-∞"},
      {start: Number.MAX_VALUE, end: -Infinity, result: en_Number_MAX_VALUE + "–-∞"},

      // Values starting at positive infinity.
      {start: Infinity, end: Number.MAX_VALUE, result: "∞–" + en_Number_MAX_VALUE},
      {start: Infinity, end: 0, result: "∞–0"},
      {start: Infinity, end: -0, result: "∞–-0"},
      {start: Infinity, end: -Number.MAX_VALUE, result: "∞–-" + en_Number_MAX_VALUE},
      {start: Infinity, end: -Infinity, result: "∞–-∞"},

      // Values ending at positive infinity.
      {start: Infinity, end: Infinity, result: "~∞"},

      // Non-special cases.
      {start: 1, end: 100, result: "1–100"},
      {start: -100, end: 100, result: "-100 – 100"},
      {start: -1000, end: -100, result: "-1,000 – -100"},
      {start: Math.PI, end: 123_456.789, result: "3.142–123,456.789"},
      {start: -Math.PI, end: Math.E, result: "-3.142 – 2.718"},
      {
        start: Number.MAX_SAFE_INTEGER,
        end: 9007199254740993,
        result: "9,007,199,254,740,991–9,007,199,254,740,992",
      },
      {
        start: Number.MAX_SAFE_INTEGER,
        end: "9007199254740993",
        result: "9,007,199,254,740,991–9,007,199,254,740,993",
      },

      // Start value is larger than end value.
      {start: -0, end: -0.1, result: "-0 – -0.1"},
      {start: -0, end: -Number.MAX_VALUE, result: "-0 – -" + en_Number_MAX_VALUE},
      {start: 1, end: 0, result: "1–0"},
      {start: 0, end: -1, result: "0–-1"},
      {start: 1, end: -1, result: "1–-1"},
      {start: -1, end: -2, result: "-1 – -2"},
      {start: "10e2", end: "1e-3", result: "1,000–0.001"},
      {start: "0x100", end: "1e1", result: "256–10"},
      {start: ".1e-999999", end: ".01e-999999", result: "~0"},
      {start: ".1e99999", end: "0", result: "100" + ",000".repeat(33332) + "–0"},
      // Number.MAX_VALUE is 1.7976931348623157e+308.
      {
        start: "1.7976931348623158e+308",
        end: Number.MAX_VALUE,
        result: "179,769,313,486,231,580" + ",000".repeat(97) + "–" + en_Number_MAX_VALUE,
      },
      // Number.MIN_VALUE is 5e-324.
      {start: "6e-324", end: Number.MIN_VALUE, result: "~0"},
    ],
  },
  "de": {
    options: {style: "currency", currency: "EUR"},
    ranges: [
      // Values around zero.
      {start: 0, end: 0, result: "≈0,00 €"},
      {start: 0, end: -0, result: "0,00 € – -0,00 €"},
      {start: -0, end: 0, result: "-0,00 € – 0,00 €"},
      {start: -0, end: 0.1e-3, result: "-0,00 € – 0,00 €"},
      {start: -0, end: "0.1e-3", result: "-0,00 € – 0,00 €"},
      {start: "-0", end: 0.1e-3, result: "-0,00 € – 0,00 €"},
      {start: -0, end: -0, result: "≈-0,00 €"},
      {start: -0, end: -0.1, result: "-0,00–0,10 €"},

      // Values starting at negative infinity.
      {start: -Infinity, end: -Infinity, result: "≈-∞ €"},
      {start: -Infinity, end: -0, result: "-∞–0,00 €"},
      {start: -Infinity, end: +0, result: "-∞ € – 0,00 €"},
      {start: -Infinity, end: +Infinity, result: "-∞ € – ∞ €"},

      // Values ending at negative infinity.
      {start: -Number.MAX_VALUE, end: -Infinity, result: "-" + de_Number_MAX_VALUE + ",00–∞ €"},
      {start: -0, end: -Infinity, result: "-0,00–∞ €"},
      {start: 0, end: -Infinity, result: "0,00 € – -∞ €"},
      {start: Number.MAX_VALUE, end: -Infinity, result: de_Number_MAX_VALUE + ",00 € – -∞ €"},

      // Values starting at positive infinity.
      {start: Infinity, end: Number.MAX_VALUE, result: "∞–" + de_Number_MAX_VALUE + ",00 €"},
      {start: Infinity, end: 0, result: "∞–0,00 €"},
      {start: Infinity, end: -0, result: "∞ € – -0,00 €"},
      {start: Infinity, end: -Number.MAX_VALUE, result: "∞ € – -" + de_Number_MAX_VALUE + ",00 €"},
      {start: Infinity, end: -Infinity, result: "∞ € – -∞ €"},

      // Values ending at positive infinity.
      {start: Infinity, end: Infinity, result: "≈∞ €"},

      // Non-special cases.
      {start: 1, end: 100, result: "1,00–100,00 €"},
      {start: -100, end: 100, result: "-100,00 € – 100,00 €"},
      {start: -1000, end: -100, result: "-1.000,00–100,00 €"},
      {start: Math.PI, end: 123_456.789, result: "3,14–123.456,79 €"},
      {start: -Math.PI, end: Math.E, result: "-3,14 € – 2,72 €"},
      {
        start: Number.MAX_SAFE_INTEGER,
        end: 9007199254740993,
        result: "9.007.199.254.740.991,00–9.007.199.254.740.992,00 €",
      },
      {
        start: Number.MAX_SAFE_INTEGER,
        end: "9007199254740993",
        result: "9.007.199.254.740.991,00–9.007.199.254.740.993,00 €",
      },

      // Start value is larger than end value.
      {start: -0, end: -0.1, result: "-0,00–0,10 €"},
      {start: -0, end: -Number.MAX_VALUE, result: "-0,00–" + de_Number_MAX_VALUE + ",00 €"},
      {start: 1, end: 0, result: "1,00–0,00 €"},
      {start: 0, end: -1, result: "0,00 € – -1,00 €"},
      {start: 1, end: -1, result: "1,00 € – -1,00 €"},
      {start: -1, end: -2, result: "-1,00–2,00 €"},
      {start: "10e2", end: "1e-3", result: "1.000,00–0,00 €"},
      {start: "0x100", end: "1e1", result: "256,00–10,00 €"},
      {start: ".1e-999999", end: ".01e-999999", result: "≈0,00 €"},
      {start: ".1e99999", end: "0", result: "100" + ".000".repeat(33332) + ",00–0,00 €"},
      // Number.MAX_VALUE is 1.7976931348623157e+308.
      {
        start: "1.7976931348623158e+308",
        end: Number.MAX_VALUE,
        result: "179.769.313.486.231.580" + ".000".repeat(97) + ",00–" + de_Number_MAX_VALUE + ",00 €",
      },
      // Number.MIN_VALUE is 5e-324.
      {start: "6e-324", end: Number.MIN_VALUE, result: "≈0,00 €"},
    ],
  },
  "fr": {
    options: {style: "unit", unit: "meter"},
    ranges: [
      // Values around zero.
      {start: 0, end: 0, result: "≃0 m"},
      {start: -0, end: 0, result: "-0 – 0 m"},
      {start: -0, end: 0, result: "-0 – 0 m"},
      {start: -0, end: 0.1e-3, result: "-0 – 0 m"},
      {start: -0, end: "0.1e-3", result: "-0 – 0 m"},
      {start: "-0", end: 0.1e-3, result: "-0 – 0 m"},
      {start: -0, end: -0, result: "≃-0 m"},
      {start: -0, end: -0.1, result: "-0 – -0,1 m"},

      // Values starting at negative infinity.
      {start: -Infinity, end: -Infinity, result: "≃-∞ m"},
      {start: -Infinity, end: -0, result: "-∞ – -0 m"},
      {start: -Infinity, end: +0, result: "-∞ – 0 m"},
      {start: -Infinity, end: +Infinity, result: "-∞ – ∞ m"},

      // Values ending at negative infinity.
      {start: -Number.MAX_VALUE, end: -Infinity, result: "-" + fr_Number_MAX_VALUE + " – -∞ m"},
      {start: -0, end: -Infinity, result: "-0 – -∞ m"},
      {start: 0, end: -Infinity, result: "0–-∞ m"},
      {start: Number.MAX_VALUE, end: -Infinity, result: fr_Number_MAX_VALUE + "–-∞ m"},

      // Values starting at positive infinity.
      {start: Infinity, end: Number.MAX_VALUE, result: "∞–" + fr_Number_MAX_VALUE + " m"},
      {start: Infinity, end: 0, result: "∞–0 m"},
      {start: Infinity, end: -0, result: "∞–-0 m"},
      {start: Infinity, end: -Number.MAX_VALUE, result: "∞–-" + fr_Number_MAX_VALUE + " m"},
      {start: Infinity, end: -Infinity, result: "∞–-∞ m"},

      // Values ending at positive infinity.
      {start: Infinity, end: Infinity, result: "≃∞ m"},

      // Non-special cases.
      {start: 1, end: 100, result: "1–100 m"},
      {start: -100, end: 100, result: "-100 – 100 m"},
      {start: -1000, end: -100, result: "-1 000 – -100 m"},
      {start: Math.PI, end: 123_456.789, result: "3,142–123 456,789 m"},
      {start: -Math.PI, end: Math.E, result: "-3,142 – 2,718 m"},
      {
        start: Number.MAX_SAFE_INTEGER,
        end: 9007199254740993,
        result: "9 007 199 254 740 991–9 007 199 254 740 992 m",
      },
      {
        start: Number.MAX_SAFE_INTEGER,
        end: "9007199254740993",
        result: "9 007 199 254 740 991–9 007 199 254 740 993 m",
      },

      // Start value is larger than end value.
      {start: -0, end: -0.1, result: "-0 – -0,1 m"},
      {start: -0, end: -Number.MAX_VALUE, result: "-0 – -" + fr_Number_MAX_VALUE + " m"},
      {start: 1, end: 0, result: "1–0 m"},
      {start: 0, end: -1, result: "0–-1 m"},
      {start: 1, end: -1, result: "1–-1 m"},
      {start: -1, end: -2, result: "-1 – -2 m"},
      {start: "10e2", end: "1e-3", result: "1 000–0,001 m"},
      {start: "0x100", end: "1e1", result: "256–10 m"},
      {start: ".1e-999999", end: ".01e-999999", result: "≃0 m"},
      {start: ".1e99999", end: "0", result: "100" + " 000".repeat(33332) + "–0 m"},
      // Number.MAX_VALUE is 1.7976931348623157e+308.
      {
        start: "1.7976931348623158e+308",
        end: Number.MAX_VALUE,
        result: "179 769 313 486 231 580" + " 000".repeat(97) + "–" + fr_Number_MAX_VALUE + " m",
      },
      // Number.MIN_VALUE is 5e-324.
      {start: "6e-324", end: Number.MIN_VALUE, result: "≃0 m"},
    ],
  },
  // Non-ASCII digits.
  "ar": {
    options: {},
    ranges: [
      {start: -2, end: -1, result: "؜-٢–١"},
      {start: -1, end: -1, result: "~؜-١"},
      {start: -1, end: 0, result: "؜-١ – ٠"},
      {start: 0, end: 0, result: "~٠"},
      {start: 0, end: 1, result: "٠–١"},
      {start: 1, end: 1, result: "~١"},
      {start: 1, end: 2, result: "١–٢"},
    ],
  },
  "th-u-nu-thai": {
    options: {},
    ranges: [
      {start: -2, end: -1, result: "-๒ - -๑"},
      {start: -1, end: -1, result: "~-๑"},
      {start: -1, end: 0, result: "-๑ - ๐"},
      {start: 0, end: 0, result: "~๐"},
      {start: 0, end: 1, result: "๐-๑"},
      {start: 1, end: 1, result: "~๑"},
      {start: 1, end: 2, result: "๑-๒"},
    ],
  },
  // Approximation sign may consist of multiple characters.
  "no": {
    options: {},
    ranges: [
      {start: 1, end: 1, result: "ca.1"},
    ],
  },
  // Approximation sign can be a word.
  "ja": {
    options: {},
    ranges: [
      {start: 1, end: 1, result: "約1"},
    ],
  },
};

for (let [locale, {options, ranges}] of Object.entries(tests)) {
  let nf = new Intl.NumberFormat(locale, options);
  for (let {start, end, result} of ranges) {
    assertEq(nf.formatRange(start, end), result, `${start}-${end}`);
    assertEq(nf.formatRangeToParts(start, end).reduce((acc, part) => acc + part.value, ""),
             result, `${start}-${end}`);
  }
}

// Throws an error if either value is NaN.
{
  const errorTests = [
    {start: NaN, end: NaN},
    {start: 0, end: NaN},
    {start: NaN, end: 0},
    {start: Infinity, end: NaN},
    {start: NaN, end: Infinity},
  ];

  let nf = new Intl.NumberFormat("en");
  for (let {start, end} of errorTests) {
    assertThrowsInstanceOf(() => nf.formatRange(start, end), RangeError);
    assertThrowsInstanceOf(() => nf.formatRangeToParts(start, end), RangeError);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
