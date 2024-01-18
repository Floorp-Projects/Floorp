/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");

/**
 * Utils for utils, by utils
 * @module utils/utils
 */

/**
 * @memberof utils/utils
 * @static
 */
export function handleError(err) {
  console.log("ERROR: ", err);
}

/**
 * @memberof utils/utils
 * @static
 */
export function promisify(context, method, ...args) {
  return new Promise((resolve, reject) => {
    args.push(response => {
      if (response.error) {
        reject(response);
      } else {
        resolve(response);
      }
    });
    method.apply(context, args);
  });
}

/**
 * @memberof utils/utils
 * @static
 */
export function endTruncateStr(str, size) {
  if (str.length > size) {
    return `…${str.slice(str.length - size)}`;
  }
  return str;
}

export function waitForMs(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

export async function saveAsLocalFile(content, fileName) {
  if (content.type !== "text") {
    return null;
  }

  const data = new TextEncoder().encode(content.value);
  return DevToolsUtils.saveAs(window, data, fileName);
}
