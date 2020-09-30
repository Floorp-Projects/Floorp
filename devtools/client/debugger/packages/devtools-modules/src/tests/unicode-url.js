/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { getUnicodeUrl, getUnicodeUrlPath, getUnicodeHostname } =
  require("../unicode-url");

describe("unicode-url", () => {
  // List of URLs used to test Unicode URL conversion
  const TEST_URLS = [
    // Type:     Readable ASCII URLs
    // Expected: All of Unicode versions should equal to the raw.
    {
      raw: "https://example.org",
      expectedUnicode: "https://example.org",
    },
    {
      raw: "http://example.org",
      expectedUnicode: "http://example.org",
    },
    {
      raw: "ftp://example.org",
      expectedUnicode: "ftp://example.org",
    },
    {
      raw: "https://example.org.",
      expectedUnicode: "https://example.org.",
    },
    {
      raw: "https://example.org/",
      expectedUnicode: "https://example.org/",
    },
    {
      raw: "https://example.org/test",
      expectedUnicode: "https://example.org/test",
    },
    {
      raw: "https://example.org/test.html",
      expectedUnicode: "https://example.org/test.html",
    },
    {
      raw: "https://example.org/test.html?one=1&two=2",
      expectedUnicode: "https://example.org/test.html?one=1&two=2",
    },
    {
      raw: "https://example.org/test.html#here",
      expectedUnicode: "https://example.org/test.html#here",
    },
    {
      raw: "https://example.org/test.html?one=1&two=2#here",
      expectedUnicode: "https://example.org/test.html?one=1&two=2#here",
    },
    // Type:     Unreadable URLs with either Punycode domain names or URI-encoded
    //           paths
    // Expected: Unreadable domain names and URI-encoded paths should be converted
    //           to readable Unicode.
    {
      raw: "https://xn--g6w.xn--8pv/test.html",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "https://\u6e2c.\u672c/test.html",
    },
    {
      raw: "https://example.org/%E6%B8%AC%E8%A9%A6.html",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "https://example.org/\u6e2c\u8a66.html",
    },
    {
      raw: "https://example.org/test.html?One=%E4%B8%80",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "https://example.org/test.html?One=\u4e00",
    },
    {
      raw: "https://example.org/test.html?%E4%B8%80=1",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "https://example.org/test.html?\u4e00=1",
    },
    {
      raw: "https://xn--g6w.xn--8pv/%E6%B8%AC%E8%A9%A6.html" +
           "?%E4%B8%80=%E4%B8%80" +
           "#%E6%AD%A4",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "https://\u6e2c.\u672c/\u6e2c\u8a66.html" +
                       "?\u4e00=\u4e00" +
                       "#\u6b64",
    },
    // Type:     data: URIs
    // Expected: All should not be converted.
    {
      raw: "data:text/plain;charset=UTF-8;Hello%20world",
      expectedUnicode: "data:text/plain;charset=UTF-8;Hello%20world",
    },
    {
      raw: "data:text/plain;charset=UTF-8;%E6%B8%AC%20%E8%A9%A6",
      expectedUnicode: "data:text/plain;charset=UTF-8;%E6%B8%AC%20%E8%A9%A6",
    },
    {
      raw: "data:image/png;base64,iVBORw0KGgoAAA" +
           "ANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4" +
           "//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU" +
           "5ErkJggg==",
      expectedUnicode: "data:image/png;base64,iVBORw0KGgoAAA" +
                       "ANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4" +
                       "//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU" +
                       "5ErkJggg==",
    },
    // Type:     Malformed URLs
    // Expected: All should not be converted.
    {
      raw: "://example.org/test",
      expectedUnicode: "://example.org/test",
    },
    {
      raw: "://xn--g6w.xn--8pv/%E6%B8%AC%E8%A9%A6.html" +
           "?%E4%B8%80=%E4%B8%80",
      expectedUnicode: "://xn--g6w.xn--8pv/%E6%B8%AC%E8%A9%A6.html" +
                       "?%E4%B8%80=%E4%B8%80",
    },
    {
      // %E8%A9 isn't a valid UTF-8 code, so this URL is malformed.
      raw: "https://xn--g6w.xn--8pv/%E6%B8%AC%E8%A9",
      expectedUnicode: "https://xn--g6w.xn--8pv/%E6%B8%AC%E8%A9",
    },
  ];

  // List of hostanmes used to test Unicode hostname conversion
  const TEST_HOSTNAMES = [
    // Type:     Readable ASCII hostnames
    // Expected: All of Unicode versions should equal to the raw.
    {
      raw: "example",
      expectedUnicode: "example",
    },
    {
      raw: "example.org",
      expectedUnicode: "example.org",
    },
    // Type:     Unreadable Punycode hostnames
    // Expected: Punycode should be converted to readable Unicode.
    {
      raw: "xn--g6w",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "\u6e2c",
    },
    {
      raw: "xn--g6w.xn--8pv",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "\u6e2c.\u672c",
    },
  ];

  // List of URL paths used to test Unicode URL path conversion
  const TEST_URL_PATHS = [
    // Type:     Readable ASCII URL paths
    // Expected: All of Unicode versions should equal to the raw.
    {
      raw: "test",
      expectedUnicode: "test",
    },
    {
      raw: "/",
      expectedUnicode: "/",
    },
    {
      raw: "/test",
      expectedUnicode: "/test",
    },
    {
      raw: "/test.html?one=1&two=2#here",
      expectedUnicode: "/test.html?one=1&two=2#here",
    },
    // Type:     Unreadable URI-encoded URL paths
    // Expected: URL paths should be converted to readable Unicode.
    {
      raw: "/%E6%B8%AC%E8%A9%A6",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "/\u6e2c\u8a66",
    },
    {
      raw: "/%E6%B8%AC%E8%A9%A6.html",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "/\u6e2c\u8a66.html",
    },
    {
      raw: "/%E6%B8%AC%E8%A9%A6.html" +
           "?%E4%B8%80=%E4%B8%80&%E4%BA%8C=%E4%BA%8C" +
           "#%E6%AD%A4",
      // Do not type Unicode characters directly, because this test file isn't
      // specified with a known encoding.
      expectedUnicode: "/\u6e2c\u8a66.html" +
                       "?\u4e00=\u4e00&\u4e8c=\u4e8c" +
                       "#\u6b64",
    },
    // Type:     Malformed URL paths
    // Expected: All should not be converted.
    {
      // %E8%A9 isn't a valid UTF-8 code, so this URL is malformed.
      raw: "/%E6%B8%AC%E8%A9",
      expectedUnicode: "/%E6%B8%AC%E8%A9",
    },
  ];

  it("Get Unicode URLs", () => {
    for (let url of TEST_URLS) {
      expect(getUnicodeUrl(url.raw)).toBe(url.expectedUnicode);
    }
  });

  it("Get Unicode hostnames", () => {
    for (let hostname of TEST_HOSTNAMES) {
      expect(getUnicodeHostname(hostname.raw)).toBe(hostname.expectedUnicode);
    }
  });

  it("Get Unicode URL paths", () => {
    for (let urlPath of TEST_URL_PATHS) {
      expect(getUnicodeUrlPath(urlPath.raw)).toBe(urlPath.expectedUnicode);
    }
  });
});
