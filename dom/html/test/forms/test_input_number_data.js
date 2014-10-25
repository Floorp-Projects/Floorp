
var tests = [
  { desc: "British English",
    langTag: "en-GB", inputWithGrouping: "123,456.78",
    inputWithoutGrouping: "123456.78", value: 123456.78
  },
  { desc: "Farsi",
    langTag: "fa", inputWithGrouping: "۱۲۳٬۴۵۶٫۷۸",
    inputWithoutGrouping: "۱۲۳۴۵۶٫۷۸", value: 123456.78
  },
  { desc: "French",
    langTag: "fr-FR", inputWithGrouping: "123 456,78",
    inputWithoutGrouping: "123456,78", value: 123456.78
  },
  { desc: "German",
    langTag: "de", inputWithGrouping: "123.456,78",
    inputWithoutGrouping: "123456,78", value: 123456.78
  },
  // Extra german test to check that a locale that uses '.' as its grouping
  // separator doesn't result in it being invalid (due to step mismatch) due
  // to the de-localization code mishandling numbers that look like other
  // numbers formatted for English speakers (i.e. treating this as 123.456
  // instead of 123456):
  { desc: "German (test 2)",
    langTag: "de", inputWithGrouping: "123.456",
    inputWithoutGrouping: "123456", value: 123456
  },
  { desc: "Hebrew",
    langTag: "he", inputWithGrouping: "123,456.78",
    inputWithoutGrouping: "123456.78", value: 123456.78
  },
];
