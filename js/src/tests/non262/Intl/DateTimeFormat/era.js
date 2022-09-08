// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
  Era, Year, Month, Day, Literal
} = DateTimeFormatParts;

const tests = {
  "en": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Month("1"), Literal("/"), Day("1"), Literal("/"), Year("1970"), Literal(" "), Era("AD")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Month("1"), Literal("/"), Day("1"), Literal("/"), Year("1971"), Literal(" "), Era("BC")
          ],
        },
      ],
    },
  ],
  "en-001": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Day("1"), Literal("/"), Month("1"), Literal("/"), Year("1970"), Literal(" "), Era("AD")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Day("1"), Literal("/"), Month("1"), Literal("/"), Year("1971"), Literal(" "), Era("BC")
          ],
        },
      ],
    },
  ],
  "de": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Day("01"), Literal("."), Month("01"), Literal("."), Year("1970"), Literal(" "), Era("n. Chr.")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Day("01"), Literal("."), Month("01"), Literal("."), Year("1971"), Literal(" "), Era("v. Chr.")
          ],
        },
      ],
    },
  ],
  "fr": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Day("01"), Literal("/"), Month("01"), Literal("/"), Year("1970"), Literal(" "), Era("ap. J.-C.")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Day("01"), Literal("/"), Month("01"), Literal("/"), Year("1971"), Literal(" "), Era("av. J.-C.")
          ],
        },
      ],
    },
  ],
  "es": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Day("1"), Literal("/"), Month("1"), Literal("/"), Year("1970"), Literal(" "), Era("d. C.")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Day("1"), Literal("/"), Month("1"), Literal("/"), Year("1971"), Literal(" "), Era("a. C.")
          ],
        },
      ],
    },
  ],
  "nl": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Day("1"), Literal("/"), Month("1"), Literal("/"), Year("1970"), Literal(" "), Era("n.Chr.")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Day("1"), Literal("/"), Month("1"), Literal("/"), Year("1971"), Literal(" "), Era("v.Chr.")
          ],
        },
      ],
    },
  ],
  "ja": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Era("西暦"), Year("1970"), Literal("/"), Month("1"), Literal("/"), Day("1")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Era("紀元前"), Year("1971"), Literal("/"), Month("1"), Literal("/"), Day("1")
          ],
        },
      ],
    },
  ],
  "zh": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Era("公元"), Literal(" "), Year("1970"), Literal("-"), Month("01"), Literal("-"), Day("01")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Era("公元前"), Literal(" "), Year("1971"), Literal("-"), Month("01"), Literal("-"), Day("01")
          ],
        },
      ],
    },
  ],
  "ar": [
    {
      options: {
        day: "numeric",
        month: "numeric",
        year: "numeric",
        era: "short",
        timeZone: "UTC",
      },
      dates: [
        {
          date: new Date("1970-01-01T00:00:00.000Z"),
          parts: [
            Day("٠١"), Literal("-"), Month("٠١"), Literal("-"), Year("١٩٧٠"), Literal(" "), Era("م")
          ],
        },
        {
          date: new Date("-001970-01-01T00:00:00.000Z"),
          parts: [
            Day("٠١"), Literal("-"), Month("٠١"), Literal("-"), Year("١٩٧١"), Literal(" "), Era("ق.م")
          ],
        },
      ],
    },
  ],
};

for (let [locale, inputs] of Object.entries(tests)) {
  for (let {options, dates} of inputs) {
    let dtf = new Intl.DateTimeFormat(locale, options);
    for (let {date, parts} of dates) {
      assertParts(dtf, date, parts);
     }
  }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
