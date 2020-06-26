/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { stripQuery, parse } from "../url";

describe("url", () => {
  describe("stripQuery", () => {
    it("strips properly", () => {
      expect(stripQuery("/file/path")).toBe("/file/path");
      expect(stripQuery("/file/path?param")).toBe("/file/path");
      expect(stripQuery("/file/path#hash")).toBe("/file/path#hash");
      expect(stripQuery("/file/path?param#hash")).toBe("/file/path#hash");
    });
  });

  describe("parse", () => {
    it("parses an absolute URL", () => {
      const val = parse("http://example.com:8080/path/file.js");

      expect(val.protocol).toBe("http:");
      expect(val.host).toBe("example.com:8080");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("");
      expect(val.hash).toBe("");
    });

    it("parses an absolute URL with query params", () => {
      const val = parse("http://example.com:8080/path/file.js?param");

      expect(val.protocol).toBe("http:");
      expect(val.host).toBe("example.com:8080");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("?param");
      expect(val.hash).toBe("");
    });

    it("parses an absolute URL with a fragment", () => {
      const val = parse("http://example.com:8080/path/file.js#hash");

      expect(val.protocol).toBe("http:");
      expect(val.host).toBe("example.com:8080");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("");
      expect(val.hash).toBe("#hash");
    });

    it("parses an absolute URL with query params and a fragment", () => {
      const val = parse("http://example.com:8080/path/file.js?param#hash");

      expect(val.protocol).toBe("http:");
      expect(val.host).toBe("example.com:8080");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("?param");
      expect(val.hash).toBe("#hash");
    });

    it("parses a partial URL", () => {
      const val = parse("/path/file.js");

      expect(val.protocol).toBe("");
      expect(val.host).toBe("");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("");
      expect(val.hash).toBe("");
    });

    it("parses a partial URL with query params", () => {
      const val = parse("/path/file.js?param");

      expect(val.protocol).toBe("");
      expect(val.host).toBe("");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("?param");
      expect(val.hash).toBe("");
    });

    it("parses a partial URL with a fragment", () => {
      const val = parse("/path/file.js#hash");

      expect(val.protocol).toBe("");
      expect(val.host).toBe("");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("");
      expect(val.hash).toBe("#hash");
    });

    it("parses a partial URL with query params and a fragment", () => {
      const val = parse("/path/file.js?param#hash");

      expect(val.protocol).toBe("");
      expect(val.host).toBe("");
      expect(val.pathname).toBe("/path/file.js");
      expect(val.search).toBe("?param");
      expect(val.hash).toBe("#hash");
    });
  });
});
