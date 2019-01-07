/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type AstPosition = { +line: number, +column: number };
export type AstLocation = { +end: AstPosition, +start: AstPosition };

export type PausePoint = {|
  types: { break: boolean, step: boolean },
  location: AstPosition,
  generatedLocation: AstPosition
|};

export type PausePointsMap = {
  [line: string]: { [column: string]: PausePoint }
};
