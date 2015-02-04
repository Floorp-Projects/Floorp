/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Note that this file is temporary workaroud until JPM is smart enough
// to cover it on it's own.

const { utils: Cu } = Components;
const rootURI = __SCRIPT_URI_SPEC__.replace("bootstrap.js", "");
const { require } = Cu.import(`${rootURI}/lib/toolkit/require.js`, {});
const { Bootstrap } = require(`${rootURI}/lib/sdk/addon/bootstrap.js`);
const { startup, shutdown, install, uninstall } = new Bootstrap(rootURI);
