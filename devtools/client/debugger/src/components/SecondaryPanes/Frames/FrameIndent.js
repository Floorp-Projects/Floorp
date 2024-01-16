/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";

export default function FrameIndent({ indentLevel = 1 } = {}) {
  // \xA0 represents the non breakable space &nbsp;
  const indentWidth = 4 * indentLevel;
  const nonBreakableSpaces = "\xA0".repeat(indentWidth);
  return React.createElement(
    "span",
    {
      className: "frame-indent clipboard-only",
    },
    nonBreakableSpaces
  );
}
