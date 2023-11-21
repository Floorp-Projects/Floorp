/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const accepted = {
  // Make sure all 12 months work
  "Jan 01 2000": "2000-01-01T00:00:00",
  "Feb 01 2000": "2000-02-01T00:00:00",
  "Mar 01 2000": "2000-03-01T00:00:00",
  "Apr 01 2000": "2000-04-01T00:00:00",
  "May 01 2000": "2000-05-01T00:00:00",
  "Jun 01 2000": "2000-06-01T00:00:00",
  "Jul 01 2000": "2000-07-01T00:00:00",
  "Aug 01 2000": "2000-08-01T00:00:00",
  "Sep 01 2000": "2000-09-01T00:00:00",
  "Oct 01 2000": "2000-10-01T00:00:00",
  "Nov 01 2000": "2000-11-01T00:00:00",
  "Dec 01 2000": "2000-12-01T00:00:00",

  // Anything after the 3rd character should be ignored
  "Janfoo 01 2000": "2000-01-01T00:00:00",
  "Febfoo 01 2000": "2000-02-01T00:00:00",
  "Marfoo 01 2000": "2000-03-01T00:00:00",
  "Aprfoo 01 2000": "2000-04-01T00:00:00",
  "Mayfoo 01 2000": "2000-05-01T00:00:00",
  "Junfoo 01 2000": "2000-06-01T00:00:00",
  "Julfoo 01 2000": "2000-07-01T00:00:00",
  "Augfoo 01 2000": "2000-08-01T00:00:00",
  "Sepfoo 01 2000": "2000-09-01T00:00:00",
  "Octfoo 01 2000": "2000-10-01T00:00:00",
  "Novfoo 01 2000": "2000-11-01T00:00:00",
  "Decfoo 01 2000": "2000-12-01T00:00:00",

  // Check different formats
  "Janfoo-01-2000": "2000-01-01T00:00:00",
  "01-Janfoo-2000": "2000-01-01T00:00:00",
  "01 Janfoo 2000": "2000-01-01T00:00:00",
};
const rejected = [
  "Foo 01 2000",

  "Ja 01 2000",
  "Fe 01 2000",
  "Ma 01 2000",
  "Ap 01 2000",
  "Ma 01 2000",
  "Ju 01 2000",
  "Au 01 2000",
  "Se 01 2000",
  "Oc 01 2000",
  "No 01 2000",
  "De 01 2000",

  "Jax 01 2000",
  "Fex 01 2000",
  "Max 01 2000",
  "Apx 01 2000",
  "Max 01 2000",
  "Jux 01 2000",
  "Aux 01 2000",
  "Sex 01 2000",
  "Ocx 01 2000",
  "Nox 01 2000",
  "Dex 01 2000",
];

for (const [test, expected] of Object.entries(accepted)) {
  const testDate = new Date(test);
  const expectedDate = new Date(expected);

  assertEq(
    false, isNaN(testDate),
    `${test} should be accepted.`
  );

  assertEq(
    testDate.getTime(), expectedDate.getTime(),
    `"${test}" should be ${expectedDate} (got ${testDate}).`
  );
}

for (const reject of rejected) {
  assertEq(
    true, isNaN(new Date(reject)),
    `"${reject}" should be rejected.`
  );
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
