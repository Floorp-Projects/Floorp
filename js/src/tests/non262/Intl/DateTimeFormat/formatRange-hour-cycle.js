// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test formatRange supports the different hour-cycle options.
//
// ICU bugs:
// https://unicode-org.atlassian.net/browse/ICU-21154
// https://unicode-org.atlassian.net/browse/ICU-21155
// https://unicode-org.atlassian.net/browse/ICU-21156
// https://unicode-org.atlassian.net/browse/ICU-21342
// https://unicode-org.atlassian.net/browse/ICU-21343

// Test locales which default to a 12-hour and a 24-hour clock.

let tests = {
  "en": [
    // Midnight to morning.
    {
      start: 0,
      end: 10,
      data: [
        "12\u2009–\u200910\u202FAM",
        "0\u2009–\u200910\u202FAM",
        "12\u2009–\u200910\u202FAM",
        "00\u2009–\u200910",
        "24\u2009–\u200910",
      ],
    },
    // Midnight to noon.
    {
      start: 0,
      end: 12,
      data: [
        "12\u202FAM\u2009–\u200912\u202FPM",
        "0\u202FAM\u2009–\u20090\u202FPM",
        "12\u202FAM\u2009–\u200912\u202FPM",
        "00\u2009–\u200912",
        "24\u2009–\u200912",
      ],
    },
    // Midnight to evening.
    {
      start: 0,
      end: 22,
      data: [
        "12\u202FAM\u2009–\u200910\u202FPM",
        "0\u202FAM\u2009–\u200910\u202FPM",
        "12\u202FAM\u2009–\u200910\u202FPM",
        "00\u2009–\u200922",
        "24\u2009–\u200922",
      ],
    },
    // Midnight to midnight.
    {
      start: 0,
      end: 24,
      data: [
        "1/1/1970, 12\u202FAM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 0\u202FAM\u2009–\u20091/2/1970, 0\u202FAM",
        "1/1/1970, 12\u202FAM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 00\u2009–\u20091/2/1970, 00",
        "1/1/1970, 24\u2009–\u20091/2/1970, 24",
      ],
    },

    // Morning to morning.
    {
      start: 1,
      end: 10,
      data: [
        "1\u2009–\u200910\u202FAM",
        "1\u2009–\u200910\u202FAM",
        "1\u2009–\u200910\u202FAM",
        "01\u2009–\u200910",
        "01\u2009–\u200910",
      ],
    },
    // Morning to noon.
    {
      start: 1,
      end: 12,
      data: [
        "1\u202FAM\u2009–\u200912\u202FPM",
        "1\u202FAM\u2009–\u20090\u202FPM",
        "1\u202FAM\u2009–\u200912\u202FPM",
        "01\u2009–\u200912",
        "01\u2009–\u200912",
      ],
    },
    // Morning to evening.
    {
      start: 1,
      end: 22,
      data: [
        "1\u202FAM\u2009–\u200910\u202FPM",
        "1\u202FAM\u2009–\u200910\u202FPM",
        "1\u202FAM\u2009–\u200910\u202FPM",
        "01\u2009–\u200922",
        "01\u2009–\u200922",
      ],
    },
    // Morning to midnight.
    {
      start: 1,
      end: 24,
      data: [
        "1/1/1970, 1\u202FAM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 1\u202FAM\u2009–\u20091/2/1970, 0\u202FAM",
        "1/1/1970, 1\u202FAM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 01\u2009–\u20091/2/1970, 00",
        "1/1/1970, 01\u2009–\u20091/2/1970, 24",
      ],
    },

    // Noon to morning.
    {
      start: 12,
      end: 24 + 1,
      data: [
        "1/1/1970, 12\u202FPM\u2009–\u20091/2/1970, 1\u202FAM",
        "1/1/1970, 0\u202FPM\u2009–\u20091/2/1970, 1\u202FAM",
        "1/1/1970, 12\u202FPM\u2009–\u20091/2/1970, 1\u202FAM",
        "1/1/1970, 12\u2009–\u20091/2/1970, 01",
        "1/1/1970, 12\u2009–\u20091/2/1970, 01",
      ],
    },
    // Noon to noon.
    {
      start: 12,
      end: 24 + 12,
      data: [
        "1/1/1970, 12\u202FPM\u2009–\u20091/2/1970, 12\u202FPM",
        "1/1/1970, 0\u202FPM\u2009–\u20091/2/1970, 0\u202FPM",
        "1/1/1970, 12\u202FPM\u2009–\u20091/2/1970, 12\u202FPM",
        "1/1/1970, 12\u2009–\u20091/2/1970, 12",
        "1/1/1970, 12\u2009–\u20091/2/1970, 12",
      ],
    },
    // Noon to evening.
    {
      start: 12,
      end: 22,
      data: [
        "12\u2009–\u200910\u202FPM",
        "0\u2009–\u200910\u202FPM",
        "12\u2009–\u200910\u202FPM",
        "12\u2009–\u200922",
        "12\u2009–\u200922",
      ],
    },
    // Noon to midnight.
    {
      start: 12,
      end: 24,
      data: [
        "1/1/1970, 12\u202FPM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 0\u202FPM\u2009–\u20091/2/1970, 0\u202FAM",
        "1/1/1970, 12\u202FPM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 12\u2009–\u20091/2/1970, 00",
        "1/1/1970, 12\u2009–\u20091/2/1970, 24",
      ],
    },

    // Evening to morning.
    {
      start: 22,
      end: 24 + 1,
      data: [
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 1\u202FAM",
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 1\u202FAM",
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 1\u202FAM",
        "1/1/1970, 22\u2009–\u20091/2/1970, 01",
        "1/1/1970, 22\u2009–\u20091/2/1970, 01",
      ],
    },
    // Evening to noon.
    {
      start: 22,
      end: 24 + 12,
      data: [
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 12\u202FPM",
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 0\u202FPM",
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 12\u202FPM",
        "1/1/1970, 22\u2009–\u20091/2/1970, 12",
        "1/1/1970, 22\u2009–\u20091/2/1970, 12",
      ],
    },
    // Evening to evening.
    {
      start: 22,
      end: 23,
      data: [
        "10\u2009–\u200911\u202FPM",
        "10\u2009–\u200911\u202FPM",
        "10\u2009–\u200911\u202FPM",
        "22\u2009–\u200923",
        "22\u2009–\u200923",
      ],
    },
    // Evening to midnight.
    {
      start: 22,
      end: 24,
      data: [
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 0\u202FAM",
        "1/1/1970, 10\u202FPM\u2009–\u20091/2/1970, 12\u202FAM",
        "1/1/1970, 22\u2009–\u20091/2/1970, 00",
        "1/1/1970, 22\u2009–\u20091/2/1970, 24",
      ],
    },
  ],

  "de": [
    // Midnight to morning.
    {
      start: 0,
      end: 10,
      data: [
        "00–10 Uhr",
        "0\u2009–\u200910 Uhr AM",
        "12\u2009–\u200910 Uhr AM",
        "00–10 Uhr",
        "24–10 Uhr",
      ],
    },
    // Midnight to noon.
    {
      start: 0,
      end: 12,
      data: [
        "00–12 Uhr",
        "0 Uhr AM\u2009–\u20090 Uhr PM",
        "12 Uhr AM\u2009–\u200912 Uhr PM",
        "00–12 Uhr",
        "24–12 Uhr",
      ],
    },
    // Midnight to evening.
    {
      start: 0,
      end: 22,
      data: [
        "00–22 Uhr",
        "0 Uhr AM\u2009–\u200910 Uhr PM",
        "12 Uhr AM\u2009–\u200910 Uhr PM",
        "00–22 Uhr",
        "24–22 Uhr",
      ],
    },
    // Midnight to midnight.
    {
      start: 0,
      end: 24,
      data: [
        "1.1.1970, 00 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 0 Uhr AM\u2009–\u20092.1.1970, 0 Uhr AM",
        "1.1.1970, 12 Uhr AM\u2009–\u20092.1.1970, 12 Uhr AM",
        "1.1.1970, 00 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 24 Uhr\u2009–\u20092.1.1970, 24 Uhr",
      ],
    },

    // Morning to morning.
    {
      start: 1,
      end: 10,
      data: [
        "01–10 Uhr",
        "1\u2009–\u200910 Uhr AM",
        "1\u2009–\u200910 Uhr AM",
        "01–10 Uhr",
        "01–10 Uhr",
      ],
    },
    // Morning to noon.
    {
      start: 1,
      end: 12,
      data: [
        "01–12 Uhr",
        "1 Uhr AM\u2009–\u20090 Uhr PM",
        "1 Uhr AM\u2009–\u200912 Uhr PM",
        "01–12 Uhr",
        "01–12 Uhr",
      ],
    },
    // Morning to evening.
    {
      start: 1,
      end: 22,
      data: [
        "01–22 Uhr",
        "1 Uhr AM\u2009–\u200910 Uhr PM",
        "1 Uhr AM\u2009–\u200910 Uhr PM",
        "01–22 Uhr",
        "01–22 Uhr",
      ],
    },
    // Morning to midnight.
    {
      start: 1,
      end: 24,
      data: [
        "1.1.1970, 01 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 1 Uhr AM\u2009–\u20092.1.1970, 0 Uhr AM",
        "1.1.1970, 1 Uhr AM\u2009–\u20092.1.1970, 12 Uhr AM",
        "1.1.1970, 01 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 01 Uhr\u2009–\u20092.1.1970, 24 Uhr",
      ],
    },

    // Noon to morning.
    {
      start: 12,
      end: 24 + 1,
      data: [
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 01 Uhr",
        "1.1.1970, 0 Uhr PM\u2009–\u20092.1.1970, 1 Uhr AM",
        "1.1.1970, 12 Uhr PM\u2009–\u20092.1.1970, 1 Uhr AM",
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 01 Uhr",
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 01 Uhr",
      ],
    },
    // Noon to noon.
    {
      start: 12,
      end: 24 + 12,
      data: [
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 12 Uhr",
        "1.1.1970, 0 Uhr PM\u2009–\u20092.1.1970, 0 Uhr PM",
        "1.1.1970, 12 Uhr PM\u2009–\u20092.1.1970, 12 Uhr PM",
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 12 Uhr",
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 12 Uhr",
      ],
    },
    // Noon to evening.
    {
      start: 12,
      end: 22,
      data: [
        "12–22 Uhr",
        "0\u2009–\u200910 Uhr PM",
        "12\u2009–\u200910 Uhr PM",
        "12–22 Uhr",
        "12–22 Uhr",
      ],
    },
    // Noon to midnight.
    {
      start: 12,
      end: 24,
      data: [
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 0 Uhr PM\u2009–\u20092.1.1970, 0 Uhr AM",
        "1.1.1970, 12 Uhr PM\u2009–\u20092.1.1970, 12 Uhr AM",
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 12 Uhr\u2009–\u20092.1.1970, 24 Uhr",
      ],
    },

    // Evening to morning.
    {
      start: 22,
      end: 24 + 1,
      data: [
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 01 Uhr",
        "1.1.1970, 10 Uhr PM\u2009–\u20092.1.1970, 1 Uhr AM",
        "1.1.1970, 10 Uhr PM\u2009–\u20092.1.1970, 1 Uhr AM",
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 01 Uhr",
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 01 Uhr",
      ],
    },
    // Evening to noon.
    {
      start: 22,
      end: 24 + 12,
      data: [
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 12 Uhr",
        "1.1.1970, 10 Uhr PM\u2009–\u20092.1.1970, 0 Uhr PM",
        "1.1.1970, 10 Uhr PM\u2009–\u20092.1.1970, 12 Uhr PM",
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 12 Uhr",
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 12 Uhr",
      ],
    },
    // Evening to evening.
    {
      start: 22,
      end: 23,
      data: [
        "22–23 Uhr",
        "10\u2009–\u200911 Uhr PM",
        "10\u2009–\u200911 Uhr PM",
        "22–23 Uhr",
        "22–23 Uhr",
      ],
    },
    // Evening to midnight.
    {
      start: 22,
      end: 24,
      data: [
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 10 Uhr PM\u2009–\u20092.1.1970, 0 Uhr AM",
        "1.1.1970, 10 Uhr PM\u2009–\u20092.1.1970, 12 Uhr AM",
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 00 Uhr",
        "1.1.1970, 22 Uhr\u2009–\u20092.1.1970, 24 Uhr",
      ],
    },
  ],
};

const hourCycles = [undefined, "h11", "h12", "h23", "h24"];
const hour12Values = [true, false];
const options = {hour: "2-digit", timeZone: "UTC"};
const hourToMillis = 60 * 60 * 1000;

for (let [locale, test] of Object.entries(tests)) {
  // Find the matching hourCycle for each hour12 value.
  let hour12Cycles = hour12Values.map(hour12 => {
    let {hourCycle} = new Intl.DateTimeFormat(locale, {...options, hour12}).resolvedOptions();
    return hourCycles.indexOf(hourCycle);
  });

  for (let {start, end, data} of Object.values(test)) {
    assertEq(data.length, hourCycles.length);

    // Test all possible "hourCycle" values, including |undefined|.
    for (let i = 0; i < hourCycles.length; i++) {
      let hourCycle = hourCycles[i];
      let expected = data[i];
      let dtf = new Intl.DateTimeFormat(locale, {...options, hourCycle});

      assertEq(dtf.formatRange(start * hourToMillis, end * hourToMillis), expected,
               `hourCycle: ${hourCycle}`);
    }

    // Test all possible "hour12" values.
    for (let i = 0; i < hour12Values.length; i++) {
      let hour12 = hour12Values[i];
      let dtf = new Intl.DateTimeFormat(locale, {...options, hour12});
      let expected = data[hour12Cycles[i]];

      assertEq(dtf.formatRange(start * hourToMillis, end * hourToMillis), expected,
               `hour12: ${hour12}`);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
