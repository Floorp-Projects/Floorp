/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createRoot, createSignal, onCleanup } from "solid-js";
import { type TPwaConfig, zPwaConfig } from "./type";
import { defaultEnabled, strDefaultConfig } from "./default-pref";
import { isRight } from "fp-ts/Either";

// Initialize default preferences if they don't exist
if (!Services.prefs.prefHasUserValue("floorp.browser.ssb.enabled")) {
  Services.prefs.setBoolPref("floorp.browser.ssb.enabled", defaultEnabled);
}

if (!Services.prefs.prefHasUserValue("floorp.browser.ssb.config")) {
  Services.prefs.setStringPref("floorp.browser.ssb.config", strDefaultConfig);
}

class PwaConfig {
  private static instance: PwaConfig;
  public enabled!: ReturnType<typeof createSignal<boolean>>;
  public config!: ReturnType<typeof createSignal<TPwaConfig>>;
  private constructor() {
    createRoot(() => {
      this.enabled = createSignal(
        Services.prefs.getBoolPref(
          "floorp.browser.ssb.enabled",
          defaultEnabled,
        ),
      );

      const configResult = zPwaConfig.decode(
        JSON.parse(
          Services.prefs.getStringPref(
            "floorp.browser.ssb.config",
            strDefaultConfig,
          ),
        ),
      );
      const initialConfig = isRight(configResult)
        ? configResult.right
        : JSON.parse(strDefaultConfig);

      this.config = createSignal<TPwaConfig>(initialConfig);

      const [enabled] = this.enabled;
      const [config] = this.config;

      createEffect(() => {
        Services.prefs.setBoolPref("floorp.browser.ssb.enabled", enabled());
      });

      createEffect(() => {
        Services.prefs.setStringPref(
          "floorp.browser.ssb.config",
          JSON.stringify(config()),
        );
      });

      const enabledObserver = () =>
        this.enabled[1](
          Services.prefs.getBoolPref("floorp.browser.ssb.enabled"),
        );
      Services.prefs.addObserver("floorp.browser.ssb.enabled", enabledObserver);
      onCleanup(() => {
        Services.prefs.removeObserver(
          "floorp.browser.ssb.enabled",
          enabledObserver,
        );
      });

      const configObserver = () => {
        const result = zPwaConfig.decode(
          JSON.parse(
            Services.prefs.getStringPref(
              "floorp.browser.ssb.config",
              strDefaultConfig,
            ),
          ),
        );
        if (isRight(result)) {
          this.config[1](result.right);
        }
      };

      Services.prefs.addObserver("floorp.browser.ssb.config", configObserver);
      onCleanup(() => {
        Services.prefs.removeObserver(
          "floorp.browser.ssb.config",
          configObserver,
        );
      });
    });
  }

  public static getInstance(): PwaConfig {
    if (!PwaConfig.instance) {
      PwaConfig.instance = new PwaConfig();
    }
    return PwaConfig.instance;
  }
}

const pwaConfig = PwaConfig.getInstance();
export const enabled = () => pwaConfig.enabled[0]();
export const setEnabled = (value: boolean) => pwaConfig.enabled[1](value);
export const config = () => pwaConfig.config[0]();
export const setConfig = (value: TPwaConfig) => pwaConfig.config[1](value);
