// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

// Tests using various locales to cover all day period types:
// "midnight", "noon", "morning1", "morning2", "afternoon1", "afternoon2",
// "evening1", "evening2", "night1", "night2".

const tests = [
  {
      // ICU doesn't support "midnight" and instead uses "night1" resp. "night2".
      // ICU bug: https://unicode-org.atlassian.net/projects/ICU/issues/ICU-12278
      date: new Date("2019-01-01T00:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "nachts", short: "nachts", long: "nachts" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "夜中", short: "夜中", long: "夜中" },
      }
  },
  {
      date: new Date("2019-01-01T03:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "nachts", short: "nachts", long: "nachts" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "夜中", short: "夜中", long: "夜中" },
      }
  },
  {
      date: new Date("2019-01-01T04:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "nachts", short: "nachts", long: "nachts" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "朝", short: "朝", long: "朝" },
      }
  },
  {
      date: new Date("2019-01-01T05:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "morgens", short: "morgens", long: "morgens" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "朝", short: "朝", long: "朝" },
      }
  },
  {
      date: new Date("2019-01-01T06:00:00"),
      locales: {
          en: { narrow: "in the morning", short: "in the morning", long: "in the morning" },
          de: { narrow: "morgens", short: "morgens", long: "morgens" },
          th: { narrow: "เช้า", short: "ในตอนเช้า", long: "ในตอนเช้า" },
          ja: { narrow: "朝", short: "朝", long: "朝" },
      }
  },
  {
      date: new Date("2019-01-01T10:00:00"),
      locales: {
          en: { narrow: "in the morning", short: "in the morning", long: "in the morning" },
          de: { narrow: "vorm.", short: "vorm.", long: "vormittags" },
          th: { narrow: "เช้า", short: "ในตอนเช้า", long: "ในตอนเช้า" },
          ja: { narrow: "朝", short: "朝", long: "朝" },
      }
  },
  {
      date: new Date("2019-01-01T12:00:00"),
      locales: {
          en: { narrow: "n", short: "noon", long: "noon" },
          de: { narrow: "mittags", short: "mittags", long: "mittags" },
          th: { narrow: "เที่ยง", short: "เที่ยง", long: "เที่ยง" },
          ja: { narrow: "正午", short: "正午", long: "正午" },
      }
  },
  {
      date: new Date("2019-01-01T13:00:00"),
      locales: {
          en: { narrow: "in the afternoon", short: "in the afternoon", long: "in the afternoon" },
          de: { narrow: "nachm.", short: "nachm.", long: "nachmittags" },
          th: { narrow: "บ่าย", short: "บ่าย", long: "บ่าย" },
          ja: { narrow: "昼", short: "昼", long: "昼" },
      }
  },
  {
      date: new Date("2019-01-01T15:00:00"),
      locales: {
          en: { narrow: "in the afternoon", short: "in the afternoon", long: "in the afternoon" },
          de: { narrow: "nachm.", short: "nachm.", long: "nachmittags" },
          th: { narrow: "บ่าย", short: "บ่าย", long: "บ่าย" },
          ja: { narrow: "昼", short: "昼", long: "昼" },
      }
  },
  {
      date: new Date("2019-01-01T16:00:00"),
      locales: {
          en: { narrow: "in the afternoon", short: "in the afternoon", long: "in the afternoon" },
          de: { narrow: "nachm.", short: "nachm.", long: "nachmittags" },
          th: { narrow: "เย็น", short: "ในตอนเย็น", long: "ในตอนเย็น" },
          ja: { narrow: "夕方", short: "夕方", long: "夕方" },
      }
  },
  {
      date: new Date("2019-01-01T18:00:00"),
      locales: {
          en: { narrow: "in the evening", short: "in the evening", long: "in the evening" },
          de: { narrow: "abends", short: "abends", long: "abends" },
          th: { narrow: "ค่ำ", short: "ค่ำ", long: "ค่ำ" },
          ja: { narrow: "夕方", short: "夕方", long: "夕方" },
      }
  },
  {
      date: new Date("2019-01-01T19:00:00"),
      locales: {
          en: { narrow: "in the evening", short: "in the evening", long: "in the evening" },
          de: { narrow: "abends", short: "abends", long: "abends" },
          th: { narrow: "ค่ำ", short: "ค่ำ", long: "ค่ำ" },
          ja: { narrow: "夜", short: "夜", long: "夜" },
      }
  },
  {
      date: new Date("2019-01-01T21:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "abends", short: "abends", long: "abends" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "夜", short: "夜", long: "夜" },
      }
  },
  {
      date: new Date("2019-01-01T22:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "abends", short: "abends", long: "abends" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "夜", short: "夜", long: "夜" },
      }
  },
  {
      date: new Date("2019-01-01T23:00:00"),
      locales: {
          en: { narrow: "at night", short: "at night", long: "at night" },
          de: { narrow: "abends", short: "abends", long: "abends" },
          th: { narrow: "กลางคืน", short: "กลางคืน", long: "กลางคืน" },
          ja: { narrow: "夜中", short: "夜中", long: "夜中" },
      }
  },
];

for (let {date, locales} of tests) {
    for (let [locale, formats] of Object.entries(locales)) {
        for (let [dayPeriod, expected] of Object.entries(formats)) {
            let dtf = new Intl.DateTimeFormat(locale, {dayPeriod});

            assertEq(dtf.format(date), expected,
                     `locale=${locale}, date=${date}, dayPeriod=${dayPeriod}`);
            assertDeepEq(dtf.formatToParts(date), [{type: "dayPeriod", value: expected}]);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
