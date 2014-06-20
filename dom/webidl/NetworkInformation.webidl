/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is https://w3c.github.io/netinfo/
 *
 * Copyright © 2014 W3C® (MIT, ERCIM, Keio, Beihang), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum ConnectionType {
    "cellular",
    "bluetooth",
    "ethernet",
    "wifi",
    "other",
    "none",
    "unknown"
};

[Pref="dom.netinfo.enabled"]
interface NetworkInformation : EventTarget {
    readonly    attribute ConnectionType type;
                attribute EventHandler   ontypechange;
};
