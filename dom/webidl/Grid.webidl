/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly]
interface Grid
{
  readonly attribute GridDimension rows;
  readonly attribute GridDimension cols;
};

[ChromeOnly]
interface GridDimension
{
  readonly attribute GridLines lines;
  readonly attribute GridTracks tracks;
};

[ChromeOnly]
interface GridLines
{
  readonly attribute unsigned long length;
  getter GridLine? item(unsigned long index);
};

[ChromeOnly]
interface GridLine
{
  readonly attribute double start;
  readonly attribute double breadth;
  readonly attribute unsigned long number;
  [Cached, Pure]
  readonly attribute sequence<DOMString> names;
};

[ChromeOnly]
interface GridTracks
{
  readonly attribute unsigned long length;
  getter GridTrack? item(unsigned long index);
};

enum GridTrackType { "explicit", "implicit" };
enum GridTrackState { "static", "repeat" };

[ChromeOnly]
interface GridTrack
{
  readonly attribute double start;
  readonly attribute double breadth;
  readonly attribute GridTrackType type;
  readonly attribute GridTrackState state;
};
