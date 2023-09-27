/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

assertEq(new Date("15AUG2015").getTime(),
         new Date(2015, Month.August, 15).getTime());

assertEq(new Date("Aug 15, 2015 10:00am").getTime(),
         new Date(2015, Month.August, 15, 10).getTime());
assertEq(new Date("Aug 15, 2015 10:00pm").getTime(),
         new Date(2015, Month.August, 15, 22).getTime());

const rejects = [
  "2023+/08/12",
  "2023/08+/12",
  "12Aug2023Sat",
  "2023/08/12 12:34:56+0900+",
  "2023/08/12 12:34:56+0900-",
  "2023/08/12 12:34:56+09:00+",
  "2023/08/12 12:34:56+09:00-",
  "2023/08/12 12:34:56 +09:00+",
  "2023/08/12 12:34:56GMT+0900,",
  "2023/08/12 12:34:56GMT+0900.",
  "2023/08/12 12:34:56GMT+0900/",
  "2023/08/12 12:34:56GMT+0900+",
  "2023/08/12 12:34:56GMT+0900-",
  "2023/08/12 12:34:56GMT+09:30,",
  "2023/08/12 12:34:56GMT+09:30.",
  "2023/08/12 12:34:56GMT+09:30/",
  "2023/08/12 12:34:56GMT+09:30+",
  "2023/08/12 12:34:56GMT+09:30-",
  "2023/08/12 12:34:56 +09:30+",
  "2023/08/12 12:34:56 GMT+09:30+",
  "2023/08/12 12:34:56.",
  "2023/08/12 12:34:56.-0900",
  "2023/08/12 12:34:56PST",
];
for (const reject of rejects) {
  assertEq(isNaN(new Date(reject)), true, `"${reject}" should be rejected.`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
