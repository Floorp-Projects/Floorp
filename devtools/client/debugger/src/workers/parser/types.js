/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import type { SourceId } from "../../types";

export type { SourceId };

export type AstSource = {|
  id: SourceId,
  isWasm: boolean,
  text: string,
  contentType: ?string,
|};

export type AstPosition = { +line: number, +column: number };
export type AstLocation = { +end: AstPosition, +start: AstPosition };
