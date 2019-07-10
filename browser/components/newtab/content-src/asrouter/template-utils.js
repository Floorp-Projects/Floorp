/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export function safeURI(url) {
  if (!url) {
    return "";
  }
  const { protocol } = new URL(url);
  const isAllowed = [
    "http:",
    "https:",
    "data:",
    "resource:",
    "chrome:",
  ].includes(protocol);
  if (!isAllowed) {
    console.warn(`The protocol ${protocol} is not allowed for template URLs.`); // eslint-disable-line no-console
  }
  return isAllowed ? url : "";
}
