// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const tests = {
  "America/Los_Angeles": {
    date: Date.UTC(2021, 5-1, 20, 12, 0, 0),
    timeZoneName: "longGeneric",
    locales: {
      "en": "5/20/2021, Pacific Time",
      "de": "20.5.2021, Nordamerikanische Westküstenzeit",
      "fr": "20/05/2021 à heure du Pacifique nord-américain",
      "ar": "٢٠‏/٥‏/٢٠٢١ توقيت المحيط الهادي",
      "th": "20/5/2564 เวลาแปซิฟิกในอเมริกาเหนือ",
      "zh": "2021/5/20 北美太平洋时间",
      "ja": "2021/5/20 アメリカ太平洋時間",
    }
  },
  "America/Los_Angeles": {
    date: Date.UTC(2021, 5-1, 20, 12, 0, 0),
    timeZoneName: "shortGeneric",
    locales: {
      "en": "5/20/2021, PT",
      "de": "20.5.2021, Los Angeles Zeit",
      "fr": "20/05/2021 à heure : Los Angeles",
      "ar": "٢٠‏/٥‏/٢٠٢١ توقيت Los Angeles",
      "th": "20/5/2564 เวลาLos Angeles",
      "zh": "2021/5/20 Los Angeles时间",
      "ja": "2021/5/20 Los Angeles時間",
    }
  },
  "America/Los_Angeles": {
    date: Date.UTC(2021, 5-1, 20, 12, 0, 0),
    timeZoneName: "longOffset",
    locales: {
      "en": "5/20/2021, GMT-07:00",
      "de": "20.5.2021, GMT-07:00",
      "fr": "20/05/2021 à UTC−07:00",
      "ar": "٢٠‏/٥‏/٢٠٢١ غرينتش-٠٧:٠٠",
      "th": "20/5/2564 GMT-07:00",
      "zh": "2021/5/20 GMT-07:00",
      "ja": "2021/5/20 GMT-07:00",
    }
  },
  "America/Los_Angeles": {
    date: Date.UTC(2021, 5-1, 20, 12, 0, 0),
    timeZoneName: "shortOffset",
    locales: {
      "en": "5/20/2021, GMT-7",
      "de": "20.5.2021, GMT-7",
      "fr": "20/05/2021, UTC−7",
      "ar": "٢٠‏/٥‏/٢٠٢١, غرينتش-٧",
      "th": "20/5/2564 GMT-7",
      "zh": "2021/5/20 GMT-7",
      "ja": "2021/5/20 GMT-7",
    }
  },
  "Europe/Berlin": {
    date: Date.UTC(2021, 5-1, 20, 12, 0, 0),
    timeZoneName: "longGeneric",
    locales: {
      "en": "5/20/2021, Central European Time",
      "de": "20.5.2021, Mitteleuropäische Zeit",
      "fr": "20/05/2021 à heure d’Europe centrale",
      "ar": "٢٠‏/٥‏/٢٠٢١ توقيت وسط أوروبا",
      "th": "20/5/2564 เวลายุโรปกลาง",
      "zh": "2021/5/20 中欧时间",
      "ja": "2021/5/20 中央ヨーロッパ時間",
    }
  },
  "Europe/Berlin": {
    date: Date.UTC(2021, 5-1, 20, 12, 0, 0),
    timeZoneName: "shortGeneric",
    locales: {
      "en": "5/20/2021, Germany Time",
      "de": "20.5.2021, MEZ",
      "fr": "20/05/2021, heure : Allemagne",
      "ar": "٢٠‏/٥‏/٢٠٢١, توقيت ألمانيا",
      "th": "20/5/2564 เวลาเยอรมนี",
      "zh": "2021/5/20 德国时间",
      "ja": "2021/5/20 ドイツ時間",
    }
  },
  "Africa/Monrovia": {
    date: Date.UTC(1971, 12-1, 6, 12, 0, 0),
    timeZoneName: "longOffset",
    locales: {
      "en": "12/6/1971, GMT-00:44:30",
      "de": "6.12.1971, GMT-00:44:30",
      "fr": "06/12/1971, UTC−00:44:30",
      "ar": "٦‏/١٢‏/١٩٧١ غرينتش-٠٠:٤٤:٣٠",
      "th": "6/12/2514 GMT-00:44:30",
      "zh": "1971/12/6 GMT-00:44:30",
      "ja": "1971/12/6 GMT-00:44:30",
    }
  },
  "Africa/Monrovia": {
    date: Date.UTC(1971, 12-1, 6, 12, 0, 0),
    timeZoneName: "shortOffset",
    locales: {
      "en": "12/6/1971, GMT-0:44:30",
      "de": "6.12.1971, GMT-0:44:30",
      "fr": "06/12/1971, UTC−0:44:30",
      "ar": "٦‏/١٢‏/١٩٧١, غرينتش-٠:٤٤:٣٠",
      "th": "6/12/2514 GMT-0:44:30",
      "zh": "1971/12/6 GMT-0:44:30",
      "ja": "1971/12/6 GMT-0:44:30",
    }
  },
};

for (let [timeZone, {timeZoneName, date, locales}] of Object.entries(tests)) {
  for (let [locale, expected] of Object.entries(locales)) {
    let dtf = new Intl.DateTimeFormat(locale, {timeZone, timeZoneName});
    assertEq(dtf.format(date), expected);
    assertEq(dtf.resolvedOptions().timeZoneName, timeZoneName);
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
