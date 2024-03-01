/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { parse } from "../../utils/url";

/**
 * Get the relative path of the url
 * Does not include any query parameters or fragment parts
 *
 * @param string url
 * @returns string path
 */
export function getRelativePath(url) {
  const { pathname } = parse(url);
  if (!pathname) {
    return url;
  }
  const index = pathname.indexOf("/");
  if (index !== -1) {
    const path = pathname.slice(index + 1);
    // If the path is empty this is likely the index file.
    // e.g http://foo.com/
    if (path == "") {
      return "(index)";
    }
    return path;
  }
  return "";
}
