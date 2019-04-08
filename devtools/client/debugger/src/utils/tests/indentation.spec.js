/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { correctIndentation, getIndentation } from "../indentation";

describe("indentation", () => {
  it("simple", () => {
    expect(
      correctIndentation(`
      foo
    `)
    ).toMatchSnapshot();
  });

  it("one line", () => {
    expect(correctIndentation("foo")).toMatchSnapshot();
  });

  it("one function", () => {
    const text = `
      function foo() {
        console.log("yo")
      }
    `;

    expect(correctIndentation(text)).toMatchSnapshot();
  });

  it("try catch", () => {
    const text = `
      try {
        console.log("yo")
      } catch (e) {
        console.log("yo")
      }
    `;

    expect(correctIndentation(text)).toMatchSnapshot();
  });

  it("mad indentation", () => {
    const text = `
      try {
        console.log("yo")
      } catch (e) {
        console.log("yo")
          }
    `;

    expect(correctIndentation(text)).toMatchSnapshot();
  });
});

describe("indentation length", () => {
  it("leading spaces", () => {
    const line = "                console.log('Hello World');";

    expect(getIndentation(line)).toEqual(16);
  });
});
