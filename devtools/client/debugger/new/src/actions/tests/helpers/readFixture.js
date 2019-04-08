/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import fs from "fs";
import path from "path";

export default function readFixture(name: string) {
  const text = fs.readFileSync(
    path.join(__dirname, `../fixtures/${name}`),
    "utf8"
  );
  return text;
}
