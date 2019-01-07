/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getURL } from "../getURL";
import { createSource } from "../../../reducers/sources";
import * as Url from "../../url";

let spy;

function createMockSource(props) {
  return createSource(
    Object.assign(
      {
        id: "server1.conn13.child1/39",
        url: "",
        sourceMapURL: "",
        isBlackBoxed: false,
        isPrettyPrinted: false,
        isWasm: false,
        text: "",
        contentType: "",
        error: "",
        loadedState: "unloaded"
      },
      props
    )
  );
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
        url: "https://a/b.js?key=randomKey"
      })
    );
    expect(urlObject.filename).toBe("b.js");
  });

  it("handles url with '#' for filename", function() {
    const urlObject = getURL(
      createMockSource({
        url: "https://a/b.js#specialSection"
      })
    );
    expect(urlObject.filename).toBe("b.js");
  });

  it("handles url with no filename for filename", function() {
    const urlObject = getURL(
      createMockSource({
        url: "https://a/c"
      })
    );
    expect(urlObject.filename).toBe("(index)");
  });

  it("separates resources by protocol and host", () => {
    const urlObject = getURL(
      createMockSource({
        url: "moz-extension://xyz/123"
      })
    );
    expect(urlObject.group).toBe("moz-extension://xyz");
  });

  it("creates a group name for webpack", () => {
    const urlObject = getURL(
      createMockSource({
        url: "webpack://src/component.jsx"
      })
    );
    expect(urlObject.group).toBe("webpack://");
  });

  describe("memoized", () => {
    beforeEach(() => {
      spy = jest.spyOn(Url, "parse");
    });

    afterEach(() => {
      spy.mockReset();
      spy.mockRestore();
    });

    it("parses a url once", () => {
      const source = createMockSource({
        url: "http://example.com/foo/bar/baz.js"
      });

      getURL(source);
      const url = getURL(source);
      expect(spy).toHaveBeenCalledTimes(1);

      expect(url).toEqual({
        filename: "baz.js",
        group: "example.com",
        path: "/foo/bar/baz.js"
      });
    });

    it("parses a url once per source", () => {
      const source = createMockSource({
        url: "http://example.com/foo/bar/baz.js"
      });
      const source2 = createMockSource({
        id: "server1.conn13.child1/40",
        url: "http://example.com/foo/bar/baz.js"
      });

      getURL(source);
      const url = getURL(source2);
      expect(spy).toHaveBeenCalledTimes(2);

      expect(url).toEqual({
        filename: "baz.js",
        group: "example.com",
        path: "/foo/bar/baz.js"
      });
    });
  });
});
