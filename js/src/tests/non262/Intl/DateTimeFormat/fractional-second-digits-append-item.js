// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
  DayPeriod, Hour, Minute, Second, FractionalSecond, Literal
} = DateTimeFormatParts

const tests = [
  // https://unicode-org.atlassian.net/browse/CLDR-13184
  // https://unicode-org.atlassian.net/browse/CLDR-13623
  {
    locale: "en",
    date: new Date("2020-01-01T00:00:00.123"),
    options: {hour: "numeric", fractionalSecondDigits: 3},
    parts: [
      Hour("12"),
      Literal(" "),
      DayPeriod("AM"),
      Literal(" (Fractional Second: "),
      FractionalSecond("123"),
      Literal(")")
    ]
  },

  // https://unicode-org.atlassian.net/browse/ICU-20992
  {
    locale: "ckb-IR",
    date: new Date("2020-01-01T00:00:00.123"),
    options: {minute: "2-digit", fractionalSecondDigits: 3},
    parts: [
      Second("٠"),
      Literal("٫"),
      FractionalSecond("١٢٣"),
      Literal(" (Minute: "),
      Minute("٠"),
      Literal(")")
    ]
  },

  // https://unicode-org.atlassian.net/browse/ICU-20992
  {
    locale: "ckb-IR",
    date: new Date("2020-01-01T00:00:00.123"),
    options: {dayPeriod: "short", fractionalSecondDigits: 3},
    parts: [
      FractionalSecond("١٢٣"),
      Literal(" (Dayperiod: "),
      DayPeriod("ب.ن"),
      Literal(")")
    ]
  },
];

for (let {locale, date, options, parts} of tests) {
  let dtf = new Intl.DateTimeFormat(locale, options);
  assertParts(dtf, date, parts);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
