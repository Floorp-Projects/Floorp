/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { memoize } from "lodash";
import { URL as URLParser } from "whatwg-url";
import type { URL } from "../types";

const defaultUrl = {
  hash: "",
  host: "",
  hostname: "",
  href: "",
  origin: "null",
  password: "",
  path: "",
  pathname: "",
  port: "",
  protocol: "",
  search: "",
  // This should be a "URLSearchParams" object
  searchParams: {},
  username: "",
};

export const parse = memoize(function parse(url: URL): any {
  try {
    const urlObj = new URLParser(url);
    (urlObj: any).path = urlObj.pathname + urlObj.search;
    return urlObj;
  } catch (err) {
    // If we're given simply a filename...
    if (url) {
      return { ...defaultUrl, path: url, pathname: url };
    }

    return defaultUrl;
  }
});

export function sameOrigin(firstUrl: URL, secondUrl: URL) {
  return parse(firstUrl).origin == parse(secondUrl).origin;
}
