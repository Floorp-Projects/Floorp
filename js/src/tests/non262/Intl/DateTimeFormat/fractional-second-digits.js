// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const {
    Second, FractionalSecond, Literal
} = DateTimeFormatParts

const tests = [
  {
      date: new Date("2019-01-01T00:00:00.123"),
      digits: {
          1: [Second("0"), Literal("."), FractionalSecond("1")],
          2: [Second("0"), Literal("."), FractionalSecond("12")],
          3: [Second("0"), Literal("."), FractionalSecond("123")],
      }
  },
  {
      date: new Date("2019-01-01T00:00:00.023"),
      digits: {
          1: [Second("0"), Literal("."), FractionalSecond("0")],
          2: [Second("0"), Literal("."), FractionalSecond("02")],
          3: [Second("0"), Literal("."), FractionalSecond("023")],
      }
  },
  {
      date: new Date("2019-01-01T00:00:00.003"),
      digits: {
          1: [Second("0"), Literal("."), FractionalSecond("0")],
          2: [Second("0"), Literal("."), FractionalSecond("00")],
          3: [Second("0"), Literal("."), FractionalSecond("003")],
      }
  },
];

for (let {date, digits} of tests) {
    for (let [fractionalSecondDigits, parts] of Object.entries(digits)) {
        let dtf = new Intl.DateTimeFormat("en", {second: "numeric", fractionalSecondDigits});

        assertParts(dtf, date, parts);
    }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
