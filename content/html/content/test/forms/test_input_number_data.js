
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
  { desc: "Hebrew",
    langTag: "he", inputWithGrouping: "123,456.78",
    inputWithoutGrouping: "123456.78", value: 123456.78
  },
];
