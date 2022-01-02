// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Era, Year, YearName, RelatedYear, Month, Day, Literal
} = DateTimeFormatParts

const tests = [
    // Test with non-leap month.
    {
        date: new Date("2020-04-23T00:00:00Z"),
        options: {},
        calendar: "chinese",
        locales: {
            "en": [Month("4"), Literal("/"), Day("1"), Literal("/"), RelatedYear("2020")],
            "de": [Day("1"), Literal("."), Month("4"), Literal("."), Year("37")],
            "ja": [YearName("庚子"), Literal("-"), Month("4"), Literal("-"), Day("1")],
            "zh": [RelatedYear("2020"), Literal("年"), Month("四月"), Day("1")],
            "ar": [RelatedYear("٢٠٢٠"), Literal("-"), Month("٠٤"), Literal("-"), Day("٠١")],
        }
    },

    // Test with leap month.
    {
        date: new Date("2020-05-23T00:00:00Z"),
        options: {},
        calendar: "chinese",
        locales: {
            "en": [Month("4bis"), Literal("/"), Day("1"), Literal("/"), RelatedYear("2020")],
            "de": [Day("1"), Literal("."), Month("4bis"), Literal("."), Year("37")],
            "ja": [YearName("庚子"), Literal("-"), Month("閏4"), Literal("-"), Day("1")],
            "zh": [RelatedYear("2020"), Literal("年"), Month("闰四月"), Day("1")],
            "ar": [RelatedYear("٢٠٢٠"), Literal("-"), Month("٠٤bis"), Literal("-"), Day("٠١")],
        }
    },

    // Display only "year" field.
    {
        date: new Date("2020-04-23T00:00:00Z"),
        options: {year: "numeric"},
        calendar: "chinese",
        locales: {
            "en": [RelatedYear("2020"), Literal("("), YearName("geng-zi"), Literal(")")],
            "de": [YearName("geng-zi")],
            "ja": [YearName("庚子"), Literal("年")],
            "zh": [RelatedYear("2020"), YearName("庚子"), Literal("年")],
            "ar": [Year("٣٧")],
        }
    },

    // Display only "month" field.
    {
        date: new Date("2020-04-23T00:00:00Z"),
        options: {month: "long"},
        calendar: "chinese",
        locales: {
            "en": [Month("Fourth Month")],
            "de": [Month("M04")],
            "ja": [Month("四月")],
            "zh": [Month("四月")],
            "ar": [Month("M04")],
        }
    },

    // Display only "month" field. (Leap month)
    {
        date: new Date("2020-05-23T00:00:00Z"),
        options: {month: "long"},
        calendar: "chinese",
        locales: {
            "en": [Month("Fourth Monthbis")],
            "de": [Month("M04bis")],
            "ja": [Month("閏四月")],
            "zh": [Month("闰四月")],
            "ar": [Month("M04bis")],
        }
    },

    // Display "year" and "month" fields.
    {
        date: new Date("2020-04-23T00:00:00Z"),
        options: {year: "numeric", month: "long"},
        calendar: "chinese",
        locales: {
            "en": [Month("Fourth Month"), Literal(" "), RelatedYear("2020"), Literal("("), YearName("geng-zi"), Literal(")")],
            "de": [Month("M04"), Literal(" "), YearName("geng-zi")],
            "ja": [YearName("庚子"), Literal("年"), Month("四月")],
            "zh": [RelatedYear("2020"), YearName("庚子"), Literal("年"), Month("四月")],
            "ar": [RelatedYear("٢٠٢٠"), Literal("("), YearName("geng-zi"), Literal(") "), Month("M04")],
        }
    },

    // Display "year" and "month" fields. (Leap month)
    {
        date: new Date("2020-05-23T00:00:00Z"),
        options: {year: "numeric", month: "long"},
        calendar: "chinese",
        locales: {
            "en": [Month("Fourth Monthbis"), Literal(" "), RelatedYear("2020"), Literal("("), YearName("geng-zi"), Literal(")")],
            "de": [Month("M04bis"), Literal(" "), YearName("geng-zi")],
            "ja": [YearName("庚子"), Literal("年"), Month("閏四月")],
            "zh": [RelatedYear("2020"), YearName("庚子"), Literal("年"), Month("闰四月")],
            "ar": [RelatedYear("٢٠٢٠"), Literal("("), YearName("geng-zi"), Literal(") "), Month("M04bis")],
        }
    },

    // Related year in traditional Korean calendar.
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "dangi",
        locales: {
            "en": [Month("11"), Literal("/"), Day("26"), Literal("/"), RelatedYear("2018")],
            "ko": [RelatedYear("2018"), Literal(". "), Month("11"), Literal(". "), Day("26"), Literal(".")],
        }
    },

    // Allowing the calendar to modify the pattern selection choice can result in falling back to
    // the root locale patterns in more cases. That can result in displaying the era field by
    // default, among other things.
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "buddhist",
        locales: {
            "en": [Month("1"), Literal("/"), Day("1"), Literal("/"), Year("2562"), Literal(" "), Era("BE")],
            "th": [Day("1"), Literal("/"), Month("1"), Literal("/"), Year("2562")],
        }
    },
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "hebrew",
        locales: {
            "en": [Day("24"), Literal(" "), Month("Tevet"), Literal(" "), Year("5779")],
            "he": [Day("24"), Literal(" ב"), Month("טבת"), Literal(" "), Year("5779")],
            "fr": [Day("24"), Literal("/"), Month("04"), Literal("/"), Year("5779"), Literal(" "), Era("A. M.")],
        }
    },
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "islamic",
        locales: {
            "en": [Month("4"), Literal("/"), Day("25"), Literal("/"), Year("1440"), Literal(" "), Era("AH")],
            "ar": [Day("٢٥"), Literal("\u200F/"), Month("٤"), Literal("\u200F/"), Year("١٤٤٠"), Literal(" "), Era("هـ")],
        }
    },
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "japanese",
        locales: {
            "en": [Month("1"), Literal("/"), Day("1"), Literal("/"), Year("31"), Literal(" "), Era("H")],
            "ja": [Era("H"), Year("31"), Literal("/"), Month("1"), Literal("/"), Day("1")],
        }
    },
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "persian",
        locales: {
            "en": [Month("10"), Literal("/"), Day("11"), Literal("/"), Year("1397"), Literal(" "), Era("AP")],
            "fa": [Year("۱۳۹۷"), Literal("/"), Month("۱۰"), Literal("/"), Day("۱۱")],
        }
    },
    {
        date: new Date("2019-01-01T00:00:00Z"),
        options: {},
        calendar: "roc",
        locales: {
            "en": [Month("1"), Literal("/"), Day("1"), Literal("/"), Year("108"), Literal(" "), Era("Minguo")],
            "zh-Hant-TW": [Era("民國"), Year("108"), Literal("/"), Month("1"), Literal("/"), Day("1")],
        }
    },
];

for (let {date, options, calendar, locales} of tests) {
    for (let [locale, result] of Object.entries(locales)) {
        let df = new Intl.DateTimeFormat(`${locale}-u-ca-${calendar}`, {timeZone: "UTC", ...options});
        assertParts(df, date, result);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
