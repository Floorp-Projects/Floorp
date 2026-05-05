/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { createRootHMR } from "#features-chrome/utils/base";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { workspaceIconTranslationKeys } from "./icon-translations.ts";

type IconTranslationsMap = Record<string, string>;

const getDefaultTranslations = (): IconTranslationsMap =>
  Object.keys(workspaceIconTranslationKeys).reduce((acc, iconName) => {
    acc[iconName] = iconName;
    return acc;
  }, {} as IconTranslationsMap);

export class IconTranslationsHandler {
  private static instance: IconTranslationsHandler | null = null;

  private iconTranslations: Signal<IconTranslationsMap>;

  public static getInstance(): IconTranslationsHandler {
    if (!IconTranslationsHandler.instance) {
      IconTranslationsHandler.instance = new IconTranslationsHandler();
    }
    return IconTranslationsHandler.instance;
  }

  private constructor() {
    this.iconTranslations = createRootHMR(
      () => {
        const sig = signal<IconTranslationsMap>(getDefaultTranslations());
        this.updateTranslations(sig);
        addI18nObserver(() => {
          this.updateTranslations(sig);
        });
        return sig;
      },
      import.meta.hot,
    );
  }

  private updateTranslations(sig: Signal<IconTranslationsMap>): void {
    const translations: IconTranslationsMap = {};
    Object.entries(workspaceIconTranslationKeys).forEach(
      ([iconName, translationKey]) => {
        translations[iconName] = i18next.t(translationKey);
      },
    );
    sig.value = translations;
  }

  public getTranslatedIconName(iconName: string): string {
    return this.iconTranslations.value[iconName] || iconName;
  }

  public getAllTranslations(): IconTranslationsMap {
    return this.iconTranslations.value;
  }
}
