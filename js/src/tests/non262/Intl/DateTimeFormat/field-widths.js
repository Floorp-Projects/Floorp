// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const date = new Date(Date.UTC(2021, 1-1, 2, 3, 4, 5, 678));

const tests = [
  // Standalone 'hour' field.
  {
    options: {
      hour: "numeric",
    },
    locales: {
      "en": "3 AM",
      "de": "03 Uhr",
    },
  },
  {
    options: {
      hour: "2-digit",
    },
    locales: {
      "en": "03 AM",
      "de": "03 Uhr",
    },
  },

  // Standalone 'minute' field.
  {
    options: {
      minute: "numeric",
    },
    locales: {
      "en": "4",
      "de": "4",
    },
  },
  {
    options: {
      minute: "2-digit",
    },
    locales: {
      "en": "04",
      "de": "04",
    },
  },

  // Standalone 'second' field.
  {
    options: {
      second: "numeric",
    },
    locales: {
      "en": "5",
      "de": "5",
    },
  },
  {
    options: {
      second: "2-digit",
    },
    locales: {
      "en": "05",
      "de": "05",
    },
  },

  // 'hour' and 'minute' fields with all possible field width combinations.
  {
    options: {
      hour: "numeric",
      minute: "numeric",
    },
    locales: {
      "en": "3:04 AM",
      "de": "03:04",
    },
  },
  {
    options: {
      hour: "numeric",
      minute: "2-digit",
    },
    locales: {
      "en": "3:04 AM",
      "de": "03:04",
    },
  },
  {
    options: {
      hour: "2-digit",
      minute: "numeric",
    },
    locales: {
      "en": "03:04 AM",
      "de": "03:04",
    },
  },
  {
    options: {
      hour: "2-digit",
      minute: "2-digit",
    },
    locales: {
      "en": "03:04 AM",
      "de": "03:04",
    },
  },

  // 'minute' and 'second' fields with all possible field width combinations.
  {
    options: {
      minute: "numeric",
      second: "numeric",
    },
    locales: {
      "en": "04:05",
      "de": "04:05",
    },
  },
  {
    options: {
      minute: "numeric",
      second: "2-digit",
    },
    locales: {
      "en": "04:05",
      "de": "04:05",
    },
  },
  {
    options: {
      minute: "2-digit",
      second: "numeric",
    },
    locales: {
      "en": "04:05",
      "de": "04:05",
    },
  },
  {
    options: {
      minute: "2-digit",
      second: "2-digit",
    },
    locales: {
      "en": "04:05",
      "de": "04:05",
    },
  },

  // Test 'hour' and 'minute' with 'hourCycle=h12'.
  {
    options: {
      hour: "numeric",
      minute: "numeric",
      hourCycle: "h12",
    },
    locales: {
      "en": "3:04 AM",
      "de": "3:04 AM",
    },
  },
  {
    options: {
      hour: "2-digit",
      minute: "2-digit",
      hourCycle: "h12",
    },
    locales: {
      "en": "03:04 AM",
      "de": "03:04 AM",
    },
  },

  // Test 'hour' and 'minute' with 'hourCycle=h23'.
  {
    options: {
      hour: "numeric",
      minute: "numeric",
      hourCycle: "h23",
    },
    locales: {
      "en": "03:04",
      "de": "03:04",
    },
  },
  {
    options: {
      hour: "2-digit",
      minute: "2-digit",
      hourCycle: "h23",
    },
    locales: {
      "en": "03:04",
      "de": "03:04",
    },
  },
];

for (let {options, locales} of tests) {
  for (let [locale, expected] of Object.entries(locales)) {
    let dtf = new Intl.DateTimeFormat(locale, {timeZone: "UTC", ...options});
    assertEq(dtf.format(date), expected);
  }
}

const toLocaleTests = {
  "en": "1/2/2021, 3:04:05 AM",
  "de": "2.1.2021, 03:04:05",
};

for (let [locale, expected] of Object.entries(toLocaleTests)) {
  assertEq(date.toLocaleString(locale, {timeZone: "UTC"}), expected);
}

const toLocaleTimeTests = {
  "en": "3:04:05 AM",
  "de": "03:04:05",
};

for (let [locale, expected] of Object.entries(toLocaleTimeTests)) {
  assertEq(date.toLocaleTimeString(locale, {timeZone: "UTC"}), expected);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
