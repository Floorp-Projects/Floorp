/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { TAB_INLINE_EDIT_PREF, TabInlineEditController } from "./controller.ts";

const LIFECYCLE_OWNER_KEY = "__floorpTabInlineEditLifecycle";

type LifecycleWindow = Window & {
  [LIFECYCLE_OWNER_KEY]?: Pick<
    TabInlineEditLifecycle,
    "destroy" | "updateLocalizedLabels"
  >;
};

export interface TabInlineEditPrefService {
  getBoolPref: (name: string, fallback: boolean) => boolean;
  addObserver: (name: string, observer: () => void) => void;
  removeObserver: (name: string, observer: () => void) => void;
}

export interface TabInlineEditLifecycleDependencies {
  prefs: TabInlineEditPrefService;
  createController: (win: Window) => TabInlineEditController;
}

function createDefaultLifecycleDependencies(): TabInlineEditLifecycleDependencies {
  return {
    prefs: {
      getBoolPref: (name, fallback) =>
        Services.prefs.getBoolPref(name, fallback),
      addObserver: (name, observer) =>
        Services.prefs.addObserver(name, observer),
      removeObserver: (name, observer) =>
        Services.prefs.removeObserver(name, observer),
    },
    createController: (win) => new TabInlineEditController(win),
  };
}

export function isTabInlineEditEnabled(
  prefs: Pick<TabInlineEditPrefService, "getBoolPref"> =
    createDefaultLifecycleDependencies().prefs,
): boolean {
  try {
    return prefs.getBoolPref(TAB_INLINE_EDIT_PREF, false) === true;
  } catch {
    return false;
  }
}

export class TabInlineEditLifecycle {
  private readonly win: LifecycleWindow;
  private readonly deps: TabInlineEditLifecycleDependencies;
  private controller: TabInlineEditController | null = null;
  private destroyed = false;
  private readonly prefObserver = () => this.syncFromPref();
  private readonly unloadObserver = () => this.destroy();

  constructor(
    win: Window,
    dependencies: Partial<TabInlineEditLifecycleDependencies> = {},
  ) {
    this.win = win as LifecycleWindow;
    this.deps = {
      ...createDefaultLifecycleDependencies(),
      ...dependencies,
    };
    const existing = this.win[LIFECYCLE_OWNER_KEY];
    if (existing && existing !== this) {
      existing.destroy();
    }
    this.win[LIFECYCLE_OWNER_KEY] = this;

    this.deps.prefs.addObserver(TAB_INLINE_EDIT_PREF, this.prefObserver);
    this.win.addEventListener("unload", this.unloadObserver, { once: true });
    this.syncFromPref();
  }

  public updateLocalizedLabels(): void {
    this.controller?.updateLocalizedLabels();
  }

  public destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;

    try {
      this.deps.prefs.removeObserver(TAB_INLINE_EDIT_PREF, this.prefObserver);
    } catch {
      // Preference service may already be shutting down with the window.
    }
    this.win.removeEventListener("unload", this.unloadObserver);
    this.controller?.destroy();
    this.controller = null;

    if (this.win[LIFECYCLE_OWNER_KEY] === this) {
      delete this.win[LIFECYCLE_OWNER_KEY];
    }
  }

  private syncFromPref(): void {
    if (this.destroyed) {
      return;
    }

    if (isTabInlineEditEnabled(this.deps.prefs)) {
      if (!this.controller) {
        this.controller = this.deps.createController(this.win);
      }
    } else {
      this.controller?.destroy();
      this.controller = null;
    }
  }
}

@noraComponent(import.meta.hot)
export default class TabInlineEdit extends NoraComponentBase {
  init(): void {
    const lifecycle = new TabInlineEditLifecycle(
      globalThis as unknown as Window,
    );
    addI18nObserver(() => lifecycle.updateLocalizedLabels());
    onCleanup(() => lifecycle.destroy());
  }
}
