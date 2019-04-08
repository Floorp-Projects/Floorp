/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type EditorPosition = { line: number, column: number };

export type EditorRange = { end: EditorPosition, start: EditorPosition };
