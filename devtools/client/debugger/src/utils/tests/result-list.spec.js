/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

jest.mock("devtools-environment", () => ({
  isFirefox: jest.fn(),
}));

import { isFirefox } from "devtools-environment";
import { scrollList } from "../result-list.js";

describe("scrollList", () => {
  it("just returns if element not found", () => {
    const li = document.createElement("li");
    scrollList([li], 1);
  });

  it("calls scrollIntoView if Firefox", () => {
    const ul = document.createElement("ul");
    const li = document.createElement("li");

    (li: any).scrollIntoView = jest.fn();
    ul.appendChild(li);
    isFirefox.mockImplementation(() => true);

    scrollList([li], 0);
    expect(li.scrollIntoView).toHaveBeenCalled();
  });

  it("sets element scrollTop if not Firefox", () => {
    const ul = document.createElement("ul");
    const li = document.createElement("li");

    ul.appendChild(li);
    isFirefox.mockImplementation(() => false);

    scrollList([li], 0);
    expect(li.scrollTop).toEqual(0);
  });
});
