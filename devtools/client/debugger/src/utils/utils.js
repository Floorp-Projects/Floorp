/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceContent } from "../types";
import { DevToolsUtils } from "devtools-modules";

/**
 * Utils for utils, by utils
 * @module utils/utils
 */

/**
 * @memberof utils/utils
 * @static
 */
export function handleError(err: any) {
  console.log("ERROR: ", err);
}

/**
 * @memberof utils/utils
 * @static
 */
export function promisify(
  context: any,
  method: any,
  ...args: any
): Promise<mixed> {
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
export function endTruncateStr(str: any, size: number) {
  if (str.length > size) {
    return `…${str.slice(str.length - size)}`;
  }
  return str;
}

export function waitForMs(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

export function downloadFile(content: SourceContent, fileName: string) {
  if (content.type !== "text") {
    return;
  }

  const data = new TextEncoder().encode(content.value);
  DevToolsUtils.saveAs(window, data, fileName);
}
