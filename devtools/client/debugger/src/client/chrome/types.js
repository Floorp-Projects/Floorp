/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type ServerLocation = {
  scriptId: string,
  lineNumber: number,
  columnNumber?: number,
};

export type Agents = {
  Debugger: any,
  Runtime: any,
  Page: any,
};

export type ChromeClientConnection = {
  connectNodeClient: () => void,
  connectNode: () => void,
};
