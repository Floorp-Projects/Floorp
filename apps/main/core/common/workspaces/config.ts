/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { zworkspacesServicesConfigs } from "./utils/type";
import { getOldConfigs } from "./old-config";

/** enable/disable workspaces */
export const [enabled, setEnabled] = createSignal(
  Services.prefs.getBoolPref("floorp.browser.workspaces.enabled", false),
);
Services.prefs.addObserver("floorp.browser.workspaces.enabled", () =>
  setEnabled(Services.prefs.getBoolPref("floorp.browser.workspaces.enabled")),
);

/** Configs */
export const [config, setConfig] = createSignal(
  zworkspacesServicesConfigs.parse(
    JSON.parse(
      Services.prefs.getStringPref("floorp.workspaces.configs", getOldConfigs),
    ),
  ),
);

createEffect(() => {
  Services.prefs.setStringPref(
    "floorp.workspaces.configs",
    JSON.stringify(config()),
  );
});

Services.prefs.addObserver("floorp.workspaces.configs", () =>
  setConfig(
    zworkspacesServicesConfigs.parse(
      JSON.parse(Services.prefs.getStringPref("floorp.workspaces.configs")),
    ),
  ),
);
