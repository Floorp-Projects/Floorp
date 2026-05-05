/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal, effect } from "@preact/signals";
import { type Signal } from "@preact/signals";
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
  public enabled!: Signal<boolean>;
  public config!: Signal<TPwaConfig>;
  private constructor() {
    this.enabled = signal(
      Services.prefs.getBoolPref(
        "floorp.browser.ssb.enabled",
        defaultEnabled,
      ),
    );

    const loadConfig = (jsonStr: string): TPwaConfig => {
      try {
        const parsed = JSON.parse(jsonStr);
        const result = zPwaConfig.decode(parsed);
        if (isRight(result)) {
          return result.right;
        }
        console.warn(
          "PWA configuration validation failed, attempting to recover partial data",
        );
        const defaultConfig = JSON.parse(strDefaultConfig);
        return { ...defaultConfig, ...parsed };
      } catch (e) {
        console.error(
          "Failed to parse PWA configuration JSON, using default",
          e,
        );
        return JSON.parse(strDefaultConfig);
      }
    };

    const initialConfig = loadConfig(
      Services.prefs.getStringPref(
        "floorp.browser.ssb.config",
        strDefaultConfig,
      ),
    );

    this.config = signal<TPwaConfig>(initialConfig);

    effect(() => {
      const v = this.enabled.value;
      Services.prefs.setBoolPref("floorp.browser.ssb.enabled", v);
    });

    effect(() => {
      const v = this.config.value;
      Services.prefs.setStringPref(
        "floorp.browser.ssb.config",
        JSON.stringify(v),
      );
    });

    const enabledObserver = () => {
      this.enabled.value = Services.prefs.getBoolPref("floorp.browser.ssb.enabled");
    };
    Services.prefs.addObserver("floorp.browser.ssb.enabled", enabledObserver);

    const configObserver = () => {
      this.config.value = loadConfig(
        Services.prefs.getStringPref(
          "floorp.browser.ssb.config",
          strDefaultConfig,
        ),
      );
    };
    Services.prefs.addObserver("floorp.browser.ssb.config", configObserver);
  }

  public static getInstance(): PwaConfig {
    if (!PwaConfig.instance) {
      PwaConfig.instance = new PwaConfig();
    }
    return PwaConfig.instance;
  }
}

const pwaConfig = PwaConfig.getInstance();
export const enabled = pwaConfig.enabled;
export const setEnabled = (value: boolean) => { pwaConfig.enabled.value = value; };
export const config = pwaConfig.config;
export const setConfig = (value: TPwaConfig) => { pwaConfig.config.value = value; }
