/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = [];

const { ActorManagerParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ActorManagerParent.sys.mjs"
);

let JSWINDOWACTORS = {
    AboutCalendar: {
        parent: {
            esModuleURI: "resource:///actors/AboutCalendarParent.sys.mjs",
        },
        child: {
            esModuleURI: "resource:///actors/AboutCalendarChild.sys.mjs",
            events: {
                DOMDocElementInserted: {},
            },
        },
        matches: ["about:calendar*"],
        remoteTypes: ["privilegedabout"],
    },
};

ActorManagerParent.addJSWindowActors(JSWINDOWACTORS);
