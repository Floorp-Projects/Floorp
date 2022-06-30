/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getDisplayURL } from "../getURL";

describe("getUrl", () => {
  it("handles normal url with http and https for filename", function() {
    const urlObject = getDisplayURL("https://a/b.js");
    expect(urlObject.filename).toBe("b.js");

    const urlObject2 = getDisplayURL("http://a/b.js");
    expect(urlObject2.filename).toBe("b.js");
  });

  it("handles url with querystring for filename", function() {
    const urlObject = getDisplayURL("https://a/b.js?key=randomKey");
    expect(urlObject.filename).toBe("b.js");
  });

  it("handles url with '#' for filename", function() {
    const urlObject = getDisplayURL("https://a/b.js#specialSection");
    expect(urlObject.filename).toBe("b.js");
  });

  it("handles url with no file extension for filename", function() {
    const urlObject = getDisplayURL("https://a/c");
    expect(urlObject.filename).toBe("c");
  });

  it("handles url with no name for filename", function() {
    const urlObject = getDisplayURL("https://a/");
    expect(urlObject.filename).toBe("(index)");
  });

  it("separates resources by protocol and host", () => {
    const urlObject = getDisplayURL("moz-extension://xyz/123");
    expect(urlObject.group).toBe("moz-extension://xyz");
  });

  it("creates a group name for webpack", () => {
    const urlObject = getDisplayURL("webpack:///src/component.jsx");
    expect(urlObject.group).toBe("webpack://");
  });

  it("creates a group name for angular source", () => {
    const urlObject = getDisplayURL("ng://src/component.jsx");
    expect(urlObject.group).toBe("ng://");
  });
});
