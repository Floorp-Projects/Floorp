// |reftest| skip-if(!this.hasOwnProperty('Intl'))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the format function with a diverse set of locales and options.

var rtf;

{
  // Numeric format
  rtf = new Intl.RelativeTimeFormat("en-US");
  assertEq(rtf.format(0, "second"), "in 0 seconds");
  assertEq(rtf.format(-0, "second"), "0 seconds ago");
  assertEq(rtf.format(-1, "second"), "1 second ago");
  assertEq(rtf.format(1, "second"), "in 1 second");

  assertEq(rtf.format(0, "minute"), "in 0 minutes");
  assertEq(rtf.format(-0, "minute"), "0 minutes ago");
  assertEq(rtf.format(-1, "minute"), "1 minute ago");
  assertEq(rtf.format(1, "minute"), "in 1 minute");

  assertEq(rtf.format(0, "hour"), "in 0 hours");
  assertEq(rtf.format(-0, "hour"), "0 hours ago");
  assertEq(rtf.format(-1, "hour"), "1 hour ago");
  assertEq(rtf.format(1, "hour"), "in 1 hour");

  assertEq(rtf.format(0, "day"), "in 0 days");
  assertEq(rtf.format(-0, "day"), "0 days ago");
  assertEq(rtf.format(-1, "day"), "1 day ago");
  assertEq(rtf.format(1, "day"), "in 1 day");

  assertEq(rtf.format(0, "week"), "in 0 weeks");
  assertEq(rtf.format(-0, "week"), "0 weeks ago");
  assertEq(rtf.format(-1, "week"), "1 week ago");
  assertEq(rtf.format(1, "week"), "in 1 week");

  assertEq(rtf.format(0, "month"), "in 0 months");
  assertEq(rtf.format(-0, "month"), "0 months ago");
  assertEq(rtf.format(-1, "month"), "1 month ago");
  assertEq(rtf.format(1, "month"), "in 1 month");

  assertEq(rtf.format(0, "year"), "in 0 years");
  assertEq(rtf.format(-0, "year"), "0 years ago");
  assertEq(rtf.format(-1, "year"), "1 year ago");
  assertEq(rtf.format(1, "year"), "in 1 year");
}

{
  // Text format
  rtf = new Intl.RelativeTimeFormat("en-US", {
    numeric: "auto"
  });
  assertEq(rtf.format(0, "second"), "now");
  assertEq(rtf.format(-0, "second"), "now");
  assertEq(rtf.format(-1, "second"), "1 second ago");
  assertEq(rtf.format(1, "second"), "in 1 second");

  assertEq(rtf.format(0, "minute"), "this minute");
  assertEq(rtf.format(-0, "minute"), "this minute");
  assertEq(rtf.format(-1, "minute"), "1 minute ago");
  assertEq(rtf.format(1, "minute"), "in 1 minute");

  assertEq(rtf.format(0, "hour"), "this hour");
  assertEq(rtf.format(-0, "hour"), "this hour");
  assertEq(rtf.format(-1, "hour"), "1 hour ago");
  assertEq(rtf.format(1, "hour"), "in 1 hour");

  assertEq(rtf.format(0, "day"), "today");
  assertEq(rtf.format(-0, "day"), "today");
  assertEq(rtf.format(-1, "day"), "yesterday");
  assertEq(rtf.format(1, "day"), "tomorrow");

  assertEq(rtf.format(0, "week"), "this week");
  assertEq(rtf.format(-0, "week"), "this week");
  assertEq(rtf.format(-1, "week"), "last week");
  assertEq(rtf.format(1, "week"), "next week");

  assertEq(rtf.format(0, "month"), "this month");
  assertEq(rtf.format(-0, "month"), "this month");
  assertEq(rtf.format(-1, "month"), "last month");
  assertEq(rtf.format(1, "month"), "next month");

  assertEq(rtf.format(0, "year"), "this year");
  assertEq(rtf.format(-0, "year"), "this year");
  assertEq(rtf.format(-1, "year"), "last year");
  assertEq(rtf.format(1, "year"), "next year");
}

{
  // Plural specifier
  rtf = new Intl.RelativeTimeFormat("en-US");
  assertEq(rtf.format(1, "seconds"), "in 1 second");
  assertEq(rtf.format(1, "minutes"), "in 1 minute");
  assertEq(rtf.format(1, "hours"), "in 1 hour");
  assertEq(rtf.format(1, "days"), "in 1 day");
  assertEq(rtf.format(1, "weeks"), "in 1 week");
  assertEq(rtf.format(1, "months"), "in 1 month");
  assertEq(rtf.format(1, "years"), "in 1 year");
}

rtf = new Intl.RelativeTimeFormat("de", {numeric: "auto"});
assertEq(rtf.format(-1, "day"), "gestern");
assertEq(rtf.format(1, "day"), "morgen");

rtf = new Intl.RelativeTimeFormat("ar", {numeric: "auto"});
assertEq(rtf.format(-1, "day"), "أمس");
assertEq(rtf.format(1, "day"), "غدًا");


rtf = new Intl.RelativeTimeFormat("en-US");

var weirdValueCases = [
  Infinity,
  -Infinity,
  NaN,
  "word",
  [0,2],
  {},
];

for (let c of weirdValueCases) {
  assertThrowsInstanceOf(() => rtf.format(c, "year"), RangeError);
};

var weirdUnitCases = [
  "test",
  "SECOND",
  "sEcOnD",
  1,
  NaN,
  undefined,
  null,
  {},
];

for (let u of weirdUnitCases) {
  assertThrowsInstanceOf(function() {
    var rtf = new Intl.RelativeTimeFormat("en-US");
    rtf.format(1, u);
  }, RangeError);
};


reportCompare(0, 0, 'ok');
