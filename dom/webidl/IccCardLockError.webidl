/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString lockType, DOMString errorName, short retryCount),
 Pref="dom.icc.enabled"]
interface IccCardLockError : DOMError {
  readonly attribute DOMString lockType;
  readonly attribute short retryCount;
};
