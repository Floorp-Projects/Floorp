// SPDX-License-Identifier: MPL-2.0

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import { onCleanup } from "solid-js";
import {
  installTranslateSwapController,
  uninstallTranslateSwapController,
} from "./controller.ts";
import type {
  SelectTranslationsPanelApi,
  TranslateSwapDocument,
} from "./types.ts";

const LABEL_KEY = "translate-swap.label";
const FALLBACK_LABEL = "Swap languages";

function localizedLabel(): string {
  const label = i18next.t(LABEL_KEY);
  return label === LABEL_KEY ? FALLBACK_LABEL : label;
}

// Firefox loads the panel script through a lazy getter, so it must only be
// touched once the panel is in use rather than during startup.
function selectTranslationsPanel(): SelectTranslationsPanelApi | null {
  return (globalThis as unknown as {
    SelectTranslationsPanel?: SelectTranslationsPanelApi;
  }).SelectTranslationsPanel ?? null;
}

@noraComponent(import.meta.hot)
export default class TranslateSwap extends NoraComponentBase {
  init(): void {
    const doc = document as TranslateSwapDocument | undefined;
    if (!doc?.documentElement) {
      console.error(
        "[translate-swap]",
        "Browser chrome is unavailable at init.",
      );
      return;
    }

    const controller = installTranslateSwapController({
      document: doc,
      getPanelApi: selectTranslationsPanel,
      label: localizedLabel(),
    });

    addI18nObserver(() => controller.setLabel(localizedLabel()));
    onCleanup(() => uninstallTranslateSwapController(controller));
  }
}
