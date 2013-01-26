/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary CFStateChangeEventDict {
  boolean success = false;
  short action = -1;
  short reason = -1;
  DOMString? number = null;
  short timeSeconds = -1;
  short serviceClass = -1;
};
