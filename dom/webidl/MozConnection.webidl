/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.network.enabled"]
interface MozConnection : EventTarget {
  readonly attribute unrestricted double bandwidth;
  readonly attribute boolean metered;

  attribute EventHandler onchange;
};

