/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";

const BREAKPOINT_SVG =
  '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 60 15" width="60" height="15"><path d="M53.07.5H1.5c-.54 0-1 .46-1 1v12c0 .54.46 1 1 1h51.57c.58 0 1.15-.26 1.53-.7l4.7-6.3-4.7-6.3c-.38-.44-.95-.7-1.53-.7z"/></svg>';
const COLUMN_MARKER_SVG =
  '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 11 13" width="11" height="13"><path d="M5.07.5H1.5c-.54 0-1 .46-1 1v10c0 .54.46 1 1 1h3.57c.58 0 1.15-.26 1.53-.7l3.7-5.3-3.7-5.3C6.22.76 5.65.5 5.07.5z"/></svg>';

type Props = {
  column: boolean
};

export default function BreakpointSvg({ column }: Props) {
  const svg = column ? COLUMN_MARKER_SVG : BREAKPOINT_SVG;

  /* eslint-disable react/no-danger */
  return <span dangerouslySetInnerHTML={{ __html: svg }} />;
}
