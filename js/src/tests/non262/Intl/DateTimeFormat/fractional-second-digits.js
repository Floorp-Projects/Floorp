// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const tests = [
  {
      date: new Date("2019-01-01T00:00:00.123"),
      digits: {
          1: {
              string: "1 (second: 0)",
              parts: [
                  {type: "fractionalSecond", value: "1"},
                  {type: "literal", value: " (second: "},
                  {type: "second", value: "0"},
                  {type: "literal", value: ")"},
              ],
          },
          2: {
              string: "0.12",
              parts: [
                  {type: "second", value: "0"},
                  {type: "literal", value: "."},
                  {type: "fractionalSecond", value: "12"},
              ],
          },
          3: {
              string: "0.123",
              parts: [
                  {type: "second", value: "0"},
                  {type: "literal", value: "."},
                  {type: "fractionalSecond", value: "123"},
              ],
          },
      }
  },
  {
      date: new Date("2019-01-01T00:00:00.023"),
      digits: {
          1: {
              string: "0 (second: 0)",
              parts: [
                  {type: "fractionalSecond", value: "0"},
                  {type: "literal", value: " (second: "},
                  {type: "second", value: "0"},
                  {type: "literal", value: ")"},
              ],
          },
          2: {
              string: "0.02",
              parts: [
                  {type: "second", value: "0"},
                  {type: "literal", value: "."},
                  {type: "fractionalSecond", value: "02"},
              ],
          },
          3: {
              string: "0.023",
              parts: [
                  {type: "second", value: "0"},
                  {type: "literal", value: "."},
                  {type: "fractionalSecond", value: "023"},
              ],
          },
      }
  },
  {
      date: new Date("2019-01-01T00:00:00.003"),
      digits: {
          1: {
              string: "0 (second: 0)",
              parts: [
                  {type: "fractionalSecond", value: "0"},
                  {type: "literal", value: " (second: "},
                  {type: "second", value: "0"},
                  {type: "literal", value: ")"},
              ],
          },
          2: {
              string: "0.00",
              parts: [
                  {type: "second", value: "0"},
                  {type: "literal", value: "."},
                  {type: "fractionalSecond", value: "00"},
              ],
          },
          3: {
              string: "0.003",
              parts: [
                  {type: "second", value: "0"},
                  {type: "literal", value: "."},
                  {type: "fractionalSecond", value: "003"},
              ],
          },
      }
  },
];

for (let {date, digits} of tests) {
    for (let [fractionalSecondDigits, {string, parts}] of Object.entries(digits)) {
        let dtf = new Intl.DateTimeFormat("en", {second: "numeric", fractionalSecondDigits});

        assertEq(dtf.format(date), string);
        assertDeepEq(dtf.formatToParts(date), parts);
    }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
