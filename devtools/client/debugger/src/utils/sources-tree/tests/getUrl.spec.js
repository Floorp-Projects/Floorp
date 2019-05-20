/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getURL } from "../getURL";
import { makeMockSource } from "../../../utils/test-mockup";
import type { Source } from "../../../types";

function createMockSource(props): Source {
  const rv = {
    ...makeMockSource(),
    ...Object.assign(
      {
        id: "server1.conn13.child1/39",
        url: "",
        sourceMapURL: "",
        isBlackBoxed: false,
        isPrettyPrinted: false,
        isWasm: false,
      },
      props
    ),
  };
  return (rv: any);
}

describe("getUrl", () => {
  it("handles normal url with http and https for filename", function() {
    const urlObject = getURL(createMockSource({ url: "https://a/b.js" }));
    expect(urlObject.filename).toBe("b.js");

    const urlObject2 = getURL(
      createMockSource({ id: "server1.conn13.child1/40", url: "http://a/b.js" })
    );
    expect(urlObject2.filename).toBe("b.js");
  });

  it("handles url with querystring for filename", function() {
    const urlObject = getURL(
      createMockSource({
        url: "https://a/b.js?key=randomKey",
      })
    );
    expect(urlObject.filename).toBe("b.js");
  });

  it("handles url with '#' for filename", function() {
    const urlObject = getURL(
      createMockSource({
        url: "https://a/b.js#specialSection",
      })
    );
    expect(urlObject.filename).toBe("b.js");
  });

  it("handles url with no file extension for filename", function() {
    const urlObject = getURL(
      createMockSource({
        url: "https://a/c",
        id: "c",
      })
    );
    expect(urlObject.filename).toBe("c");
  });

  it("handles url with no name for filename", function() {
    const urlObject = getURL(
      createMockSource({
        url: "https://a/",
        id: "c",
      })
    );
    expect(urlObject.filename).toBe("(index)");
  });

  it("separates resources by protocol and host", () => {
    const urlObject = getURL(
      createMockSource({
        url: "moz-extension://xyz/123",
        id: "c2",
      })
    );
    expect(urlObject.group).toBe("moz-extension://xyz");
  });

  it("creates a group name for webpack", () => {
    const urlObject = getURL(
      createMockSource({
        url: "webpack://src/component.jsx",
        id: "c3",
      })
    );
    expect(urlObject.group).toBe("webpack://");
  });
});
