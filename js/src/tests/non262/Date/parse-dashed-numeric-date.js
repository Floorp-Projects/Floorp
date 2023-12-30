// |reftest| skip-if(xulRuntime.OS=="WINNT") -- Windows doesn't accept IANA names for the TZ env variable
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

inTimeZone("MST", () => {
  // The upper limit is from the TimeClip algorithm:
  // https://tc39.es/ecma262/#sec-timeclip
  // https://tc39.es/ecma262/#sec-utc-t
  //
  // The result is in localtime but the limit is processed in GMT, which
  // is why these are offset from the actual limit of 275760-09-13T00:00:00
  // (in this case by 7h, because we're testing in MST)
  const accepted = {
    "19999-09-12": new Date(19999, Month.September, 12),
    "19999-9-1": new Date(19999, Month.September, 1),
    "19999-9-1 ": new Date(19999, Month.September, 1),
    "275760-09-12": new Date(275760, Month.September, 12),
    "275760-09-12 17:00:00": new Date(275760, Month.September, 12, 17),
    "19999-09-12 (comment) 23:00:00": new Date(19999, Month.September, 12, 23),
    "19999-09-12 22:00:00 GMT-0800": new Date(19999, Month.September, 12, 23),

    // Single digit mon or mday
    "2021-09-1": new Date(2021, Month.September, 1),
    "2021-9-01": new Date(2021, Month.September, 1),
    "2021-9-1": new Date(2021, Month.September, 1),

    // 1-12 for first number should be month
    "1-09-12": new Date(2012, Month.January, 9),
    "1-09-2012": new Date(2012, Month.January, 9),
    "12-09-12": new Date(2012, Month.December, 9),

    // 32-99 for first number is the year...
    // 32-49 are 2032-2049, 50-99 are 1950-1999
    "32-09-12": new Date(2032, Month.September, 12),
    "49-09-12": new Date(2049, Month.September, 12),
    "50-09-12": new Date(1950, Month.September, 12),
    "99-09-12": new Date(1999, Month.September, 12),

    // Year 0 is 2000
    "0-9-12": new Date(2000, Month.September, 12),
    "9-12-0": new Date(2000, Month.September, 12),

    // 3-digit year is also fine
    "999-09-12": new Date(999, Month.September, 12),

    // Bug 1617258 (result is -7 hours)
    "2020-03-24 12:54:40 AM +00:00": new Date(2020, Month.March, 23, 17, 54, 40),

    // Oddball time formats for Chrome parity
    "9999-09-12 :00:01": new Date(9999, Month.September, 12, 0, 1),
    // TODO: This one still does not have parity- bug 1858595
    "9999-09-12 01::01": new Date(9999, Month.September, 12, 1, 1, 0),

    // mday > # of days in mon rolls over to the next month
    // as long as mday <= 31 (2022 is not a leap year)
    // These are processed as ISO dates, therefore are in GMT
    "2022-02-29": new Date(2022, Month.February, 28, 17),
    "2022-02-30": new Date(2022, Month.March, 1, 17),
    "2022-02-31": new Date(2022, Month.March, 2, 17),

    // No space before time zone
    "19999-9-1MST": new Date(19999, Month.September, 1),
    "19999-9-1GMT-07": new Date(19999, Month.September, 1),

    // Delimiter other than space after prefix
    "19999-9-1.10:13:14": new Date(19999, Month.September, 1, 10, 13, 14),
    "19999-9-1,10:13:14": new Date(19999, Month.September, 1, 10, 13, 14),
    "19999-9-1-10:13:14": new Date(19999, Month.September, 1, 10, 13, 14),
    "19999-9-1-4:30": new Date(19999, Month.September, 1, 4, 30),
    "19999-9-1/10:13:14": new Date(19999, Month.September, 1, 10, 13, 14),
    "19999-9-1()10:13:14": new Date(19999, Month.September, 1, 10, 13, 14),
    // Open paren only comments out the time
    "19999-9-1(10:13:14": new Date(19999, Month.September, 1),
  };
  const rejected = [
    "275760-09-12 17:00:01",
    "275760-09-13",

    // Rejected delimiters after prefix
    "19999-09-12T00:00:00",
    "19999-09-12:00:00:00",
    "19999-09-12^00:00:00",
    "19999-09-12|00:00:00",
    "19999-09-12~00:00:00",
    "19999-09-12+00:00:00",
    "19999-09-12=00:00:00",
    "19999-09-12?00:00:00",

    // 13-31 for first number is invalid (after 31 can be parsed as YY-MM-DD),
    // but 32 is still no good if the last number is a YYYY
    "13-09-12",
    "13-09-2012",
    "31-09-12",
    "31-09-2012",
    "32-09-2012",

    // mday > 31 is invalid, does not roll over to the next month
    "2022-02-32",
    // month > 12
    "2022-13-30",

    // 00 for mon or mday
    "0000-00-00",
    "0000-01-00",
    "0000-00-01",
  ];

  for (const [test, dateObject] of Object.entries(accepted)) {
    const testDate = new Date(test);

    assertEq(
      false, isNaN(testDate),
      `${test} should be accepted.`
    );

    assertEq(
      testDate.getTime(), dateObject.getTime(),
      `"${test}" should be ${dateObject} (got ${testDate}).`
    );
  }

  for (const reject of rejected) {
    assertEq(
      true, isNaN(new Date(reject)),
      `"${reject}" should be rejected.`
    );
  }
});

if (typeof reportCompare === "function")
  reportCompare(true, true);
