/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { scrollList } from "../result-list.js";

describe("scrollList", () => {
  beforeEach(() => {
    jest.useFakeTimers();
  });

  /* eslint-disable jest/expect-expect */
  it("just returns if element not found", () => {
    const li = document.createElement("li");
    scrollList([li], 1);
  });
  /* eslint-enable jest/expect-expect */

  it("calls scrollIntoView", () => {
    const ul = document.createElement("ul");
    const li = document.createElement("li");

    li.scrollIntoView = jest.fn();
    ul.appendChild(li);

    scrollList([li], 0);

    jest.runAllTimers();

    expect(li.scrollIntoView).toHaveBeenCalled();
  });
});
