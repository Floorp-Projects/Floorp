// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

// Int64-BigInts which can be exactly represented as doubles receive a fast-path.

const tests = {
  "en": {
    options: {},
    ranges: [
      // BigInt around Number.MIN_SAFE_INTEGER.
      {
        start: -0x20000000000001n,
        end: -0x20000000000000n,
        result: "-9,007,199,254,740,993 – -9,007,199,254,740,992",
      },
      {
        start: -0x20000000000000n,
        end: -0x20000000000000n,
        result: "~-9,007,199,254,740,992",
      },
      {
        start: -0x20000000000000n,
        end: -0x1fffffffffffffn,
        result: "-9,007,199,254,740,992 – -9,007,199,254,740,991",
      },
      {
        start: -0x1fffffffffffffn,
        end: -0x1fffffffffffffn,
        result: "~-9,007,199,254,740,991",
      },
      {
        start: -0x1fffffffffffffn,
        end: -0x1ffffffffffffen,
        result: "-9,007,199,254,740,991 – -9,007,199,254,740,990",
      },
      {
        start: -0x1ffffffffffffen,
        end: -0x1ffffffffffffen,
        result: "~-9,007,199,254,740,990",
      },

      // BigInt around Number.MAX_SAFE_INTEGER.
      {
        start: 0x1ffffffffffffen,
        end: 0x1ffffffffffffen,
        result: "~9,007,199,254,740,990",
      },
      {
        start: 0x1ffffffffffffen,
        end: 0x1fffffffffffffn,
        result: "9,007,199,254,740,990–9,007,199,254,740,991",
      },
      {
        start: 0x1fffffffffffffn,
        end: 0x1fffffffffffffn,
        result: "~9,007,199,254,740,991",
      },
      {
        start: 0x1fffffffffffffn,
        end: 0x20000000000000n,
        result: "9,007,199,254,740,991–9,007,199,254,740,992",
      },
      {
        start: 0x20000000000000n,
        end: 0x20000000000000n,
        result: "~9,007,199,254,740,992",
      },
      {
        start: 0x20000000000000n,
        end: 0x20000000000001n,
        result: "9,007,199,254,740,992–9,007,199,254,740,993",
      },

      // BigInt around INT64_MIN.
      {
        start: -0x8000000000000002n,
        end: -0x8000000000000001n,
        result: "-9,223,372,036,854,775,810 – -9,223,372,036,854,775,809",
      },
      {
        start: -0x8000000000000001n,
        end: -0x8000000000000001n,
        result: "~-9,223,372,036,854,775,809",
      },
      {
        start: -0x8000000000000001n,
        end: -0x8000000000000000n,
        result: "-9,223,372,036,854,775,809 – -9,223,372,036,854,775,808",
      },
      {
        start: -0x8000000000000000n,
        end: -0x8000000000000000n,
        result: "~-9,223,372,036,854,775,808",
      },
      {
        start: -0x8000000000000000n,
        end: -0x7fffffffffffffffn,
        result: "-9,223,372,036,854,775,808 – -9,223,372,036,854,775,807",
      },
      {
        start: -0x7fffffffffffffffn,
        end: -0x7fffffffffffffffn,
        result: "~-9,223,372,036,854,775,807",
      },

      // BigInt around INT64_MAX.
      {
        start: 0x7ffffffffffffffen,
        end: 0x7ffffffffffffffen,
        result: "~9,223,372,036,854,775,806",
      },
      {
        start: 0x7ffffffffffffffen,
        end: 0x7fffffffffffffffn,
        result: "9,223,372,036,854,775,806–9,223,372,036,854,775,807",
      },
      {
        start: 0x7fffffffffffffffn,
        end: 0x7fffffffffffffffn,
        result: "~9,223,372,036,854,775,807",
      },
      {
        start: 0x7fffffffffffffffn,
        end: 0x8000000000000000n,
        result: "9,223,372,036,854,775,807–9,223,372,036,854,775,808",
      },
      {
        start: 0x8000000000000000n,
        end: 0x8000000000000000n,
        result: "~9,223,372,036,854,775,808",
      },
      {
        start: 0x8000000000000000n,
        end: 0x8000000000000001n,
        result: "9,223,372,036,854,775,808–9,223,372,036,854,775,809",
      },
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

if (typeof reportCompare === "function")
  reportCompare(true, true);
