/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// console.table fallback to console.log for unsupported parameters.

"use strict";

const tests = [
  [`console.table(10, 20, 30, 40, 50)`, `10 20 30 40 50`],
  [`console.table(1.2, 3.4, 5.6)`, `1.2 3.4 5.6`],
  [`console.table(10n, 20n, 30n)`, `10n 20n 30n`],
  [`console.table(true, false)`, `true false`],
  [`console.table("foo", "bar", "baz")`, `foo bar baz`],
  [`console.table(null, undefined, null)`, `null undefined null`],
  [`console.table(undefined, null, undefined)`, `undefined null undefined`],
  [`console.table(Symbol.iterator)`, `Symbol(Symbol.iterator)`],
  [`console.table(/pattern/i)`, `/pattern/i`],
  [`console.table(function f() {})`, `function f()`],
];

add_task(async function () {
  const TEST_URI = "data:text/html,<!DOCTYPE html><meta charset=utf8>";

  const hud = await openNewTabAndConsole(TEST_URI);

  for (const [input, output] of tests) {
    execute(hud, input);
    const message = await waitFor(
      () => findConsoleAPIMessage(hud, output),
      `Waiting for output for ${input}`
    );

    is(
      message.querySelector(".message-body").textContent,
      output,
      `Expected messages are displayed for ${input}`
    );
  }
});
