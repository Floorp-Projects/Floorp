// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const Start = NumberRangeFormatParts("startRange");
const End = NumberRangeFormatParts("endRange");
const Shared = NumberRangeFormatParts("shared");

const tests = {
  "en": {
    options: {},
    ranges: [
      // Approximation sign present.
      {
        start: 0,
        end: 0,
        result: [Shared.Approx("~"), Shared.Integer("0")],
      },
      {
        start: -0,
        end: -0,
        result: [Shared.Approx("~"), Shared.MinusSign("-"), Shared.Integer("0")],
      },
      {
        start: -1,
        end: -1,
        result: [Shared.Approx("~"), Shared.MinusSign("-"), Shared.Integer("1")],
      },
      {
        start: 0.5,
        end: 0.5,
        result: [Shared.Approx("~"), Shared.Integer("0"), Shared.Decimal("."), Shared.Fraction("5")],
      },
      {
        start: Infinity,
        end: Infinity,
        result: [Shared.Approx("~"), Shared.Inf("∞")],
      },
      {
        start: -Infinity,
        end: -Infinity,
        result: [Shared.Approx("~"), Shared.MinusSign("-"), Shared.Inf("∞")],
      },

      // Proper ranges.
      {
        start: -2,
        end: -1,
        result: [Start.MinusSign("-"), Start.Integer("2"), Shared.Literal(" – "), End.MinusSign("-"), End.Integer("1")],
      },
      {
        start: -1,
        end: 1,
        result: [Start.MinusSign("-"), Start.Integer("1"), Shared.Literal(" – "), End.Integer("1")],
      },
      {
        start: 1,
        end: 2,
        result: [Start.Integer("1"), Shared.Literal("–"), End.Integer("2")],
      },
    ],
  },
  // Non-ASCII digits.
  "ar": {
    options: {},
    ranges: [
      {
        start: -2,
        end: -1,
        result: [Shared.Literal("\u061C"), Shared.MinusSign("-"), Start.Integer("٢"), Shared.Literal("–"), End.Integer("١")],
      },
      {
        start: -1,
        end: -1,
        result: [Shared.Approx("~\u061C"), Shared.MinusSign("-"), Shared.Integer("١")],
      },
      {
        start: -1,
        end: 0,
        result: [Start.Literal("\u061C"), Start.MinusSign("-"), Start.Integer("١"), Shared.Literal(" – "), End.Integer("٠")],
      },
      {
        start: 0,
        end: 0,
        result: [Shared.Approx("~"), Shared.Integer("٠")],
      },
      {
        start: 0,
        end: 1,
        result: [Start.Integer("٠"), Shared.Literal("–"), End.Integer("١")],
      },
      {
        start: 1,
        end: 1,
        result: [Shared.Approx("~"), Shared.Integer("١")],
      },
      {
        start: 1,
        end: 2,
        result: [Start.Integer("١"), Shared.Literal("–"), End.Integer("٢")],
      },
    ],
  },
  "th-u-nu-thai": {
    options: {},
    ranges: [
      {
        start: -2,
        end: -1,
        result: [Start.MinusSign("-"), Start.Integer("๒"), Shared.Literal(" - "), End.MinusSign("-"), End.Integer("๑")],
      },
      {
        start: -1,
        end: -1,
        result: [Shared.Approx("~"), Shared.MinusSign("-"), Shared.Integer("๑")],
      },
      {
        start: -1,
        end: 0,
        result: [Start.MinusSign("-"), Start.Integer("๑"), Shared.Literal(" - "), End.Integer("๐")],
      },
      {
        start: 0,
        end: 0,
        result: [Shared.Approx("~"), Shared.Integer("๐")],
      },
      {
        start: 0,
        end: 1,
        result: [Start.Integer("๐"), Shared.Literal("-"), End.Integer("๑")],
      },
      {
        start: 1,
        end: 1,
        result: [Shared.Approx("~"), Shared.Integer("๑")],
      },
      {
        start: 1,
        end: 2,
        result: [Start.Integer("๑"), Shared.Literal("-"), End.Integer("๒")],
      },
    ],
  },
  // Approximation sign may consist of multiple characters.
  "no": {
    options: {},
    ranges: [
      {
        start: 1,
        end: 1,
        result: [Shared.Approx("ca. "), Shared.Integer("1")],
      },
    ],
  },
  // Approximation sign can be a word.
  "ja": {
    options: {},
    ranges: [
      {
        start: 1,
        end: 1,
        result: [Shared.Approx("約 "), Shared.Integer("1")],
      },
    ],
  },
};

for (let [locale, {options, ranges}] of Object.entries(tests)) {
  let nf = new Intl.NumberFormat(locale, options);
  for (let {start, end, result} of ranges) {
    assertRangeParts(nf, start, end, result);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
