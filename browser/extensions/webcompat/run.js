/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals AboutCompatBroker, AVAILABLE_INJECTIONS, AVAILABLE_SHIMS,
           AVAILABLE_PIP_OVERRIDES, AVAILABLE_UA_OVERRIDES, CUSTOM_FUNCTIONS,
           Injections, PictureInPictureOverrides, Shims, UAOverrides */

const injections = new Injections(AVAILABLE_INJECTIONS, CUSTOM_FUNCTIONS);
const uaOverrides = new UAOverrides(AVAILABLE_UA_OVERRIDES);
const pipOverrides = new PictureInPictureOverrides(AVAILABLE_PIP_OVERRIDES);
const shims = new Shims(AVAILABLE_SHIMS);

const aboutCompatBroker = new AboutCompatBroker({
  injections,
  uaOverrides,
});

aboutCompatBroker.bootup();
injections.bootup();
uaOverrides.bootup();
pipOverrides.bootup();
