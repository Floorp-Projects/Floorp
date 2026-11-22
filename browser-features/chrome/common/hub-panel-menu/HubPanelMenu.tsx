/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type {
  AppMenuAcknowledgement,
  AppMenuAdapter,
  AppMenuAdapterConnection,
  AppMenuOwner,
  AppMenuRemovalReason,
  AppMenuRenderRequest,
} from "../../../modules/modules/feature-host/AppMenuRegistry.sys.mts";
import type { HubPanelMenuBootstrapModule } from "../../../modules/modules/feature-host/HubPanelMenuBootstrap.sys.mts";
import {
  cancelHubMenuFocusClaim,
  mountOwnedHubPanelMenuView,
  observeHubMenuReadiness,
} from "./HubPanelMenuView.tsx";
import type {
  HubPanelMenuFocusContext,
  OwnedHubPanelMenuView,
} from "./types.ts";

const { registerHubPanelMenuAdapter, usesExperimentalHubPanelMenu } =
  ChromeUtils.importESModule(
    "resource://noraneko/modules/feature-host/HubPanelMenuBootstrap.sys.mjs",
  ) as HubPanelMenuBootstrapModule;

export class HubPanelMenu {
  readonly #window: Window;
  readonly #owned = new Map<string, OwnedHubPanelMenuView>();
  readonly #pendingFailures: { owner: AppMenuOwner; error: Error }[] = [];
  #connection: AppMenuAdapterConnection | null = null;
  #triggerObserver: MutationObserver | null = null;
  #observedTrigger: Element | null = null;
  #discoveryObserver: MutationObserver | null = null;
  readonly #readinessObservers = new Map<string, () => void>();
  #legacyRendered = false;
  #disposed = false;

  constructor(targetWindow: Window = window) {
    this.#window = targetWindow;
    onCleanup(() => this.#dispose());
    if (usesExperimentalHubPanelMenu(targetWindow)) {
      const connection = registerHubPanelMenuAdapter(
        targetWindow,
        this.#createAdapter(),
      );
      this.#connection = connection;
      if (connection) {
        this.#watchExperimentalTrigger();
        for (const failure of this.#pendingFailures.splice(0)) {
          connection.reportFailure(failure.owner, failure.error);
        }
      }
    } else {
      this.#watchMenuButton(() => this.#renderLegacy());
    }
  }

  #createAdapter(): AppMenuAdapter {
    return {
      render: (request) => this.#renderExperimental(request),
      setEnabled: (owner, enabled) => {
        this.#owned.get(this.#renderKey(owner))?.setEnabled(enabled);
      },
      remove: (request, reason) =>
        this.#removeOwned(this.#renderKey(request.owner), reason),
    };
  }

  #renderExperimental(request: AppMenuRenderRequest): AppMenuAcknowledgement {
    const key = this.#renderKey(request.owner);
    const trigger = this.#window.document.getElementById("PanelUI-menu-button");
    if (!trigger) {
      this.#watchReadiness(
        key,
        () => !!this.#window.document.getElementById("PanelUI-menu-button"),
      );
      return null;
    }
    const menuOpen = trigger.getAttribute("open") === "true";
    const parent = this.#parentElement;
    const settings = this.#settingsButton;
    const existing = this.#owned.get(key);
    if (existing?.node?.isConnected) {
      this.#stopReadiness(key);
      existing.setEnabled(request.enabled);
      return "rendered";
    }
    if (parent && settings) {
      this.#mount(
        key,
        parent,
        settings,
        request.enabled,
        () => this.#connection?.invoke(request.owner),
        request.owner,
        request.buttonId,
        request.localizationKey,
      );
      this.#stopReadiness(key);
      return "rendered";
    }
    if (menuOpen) {
      this.#watchReadiness(key, () => {
        const currentTrigger = this.#window.document.getElementById(
          "PanelUI-menu-button",
        );
        return currentTrigger?.getAttribute("open") === "true" &&
          !!this.#parentElement && !!this.#settingsButton;
      });
    }
    return menuOpen ? null : "deferred";
  }

  #watchExperimentalTrigger(): void {
    const trigger = this.#window.document.getElementById(
      "PanelUI-menu-button",
    );
    if (!trigger || trigger === this.#observedTrigger) return;
    this.#triggerObserver?.disconnect();
    this.#observedTrigger = trigger;
    this.#triggerObserver = new MutationObserver(() => {
      if (trigger.getAttribute("open") === "true") {
        this.#connection?.notifyReady();
      }
    });
    this.#triggerObserver.observe(trigger, {
      attributes: true,
      attributeFilter: ["open"],
    });
  }

  #watchMenuButton(onOpen: () => void): void {
    const attach = (): boolean => {
      const button = this.#window.document.getElementById(
        "PanelUI-menu-button",
      );
      if (!button) return false;
      this.#triggerObserver?.disconnect();
      this.#triggerObserver = new MutationObserver(() => onOpen());
      this.#triggerObserver.observe(button, {
        attributes: true,
        attributeFilter: ["open"],
      });
      onOpen();
      return true;
    };
    if (!attach()) {
      this.#discoveryObserver = new MutationObserver(() => {
        if (attach()) this.#discoveryObserver?.disconnect();
      });
      this.#discoveryObserver.observe(this.#window.document.documentElement, {
        childList: true,
        subtree: true,
      });
    }
  }

  #watchReadiness(key: string, isReady: () => boolean): void {
    if (this.#readinessObservers.has(key)) return;
    const root = this.#window.document.documentElement;
    if (!root) return;
    const stop = observeHubMenuReadiness(
      root,
      isReady,
      () => {
        this.#stopReadiness(key);
        this.#watchExperimentalTrigger();
        this.#connection?.notifyReady();
      },
    );
    this.#readinessObservers.set(key, stop);
  }

  #stopReadiness(key: string): void {
    const stop = this.#readinessObservers.get(key);
    try {
      stop?.();
    } finally {
      this.#readinessObservers.delete(key);
    }
  }

  #renderLegacy(): void {
    const button = this.#window.document.getElementById("PanelUI-menu-button");
    if (
      button?.getAttribute("open") !== "true" || this.#legacyRendered ||
      !this.#parentElement || !this.#settingsButton
    ) return;
    this.#mount(
      "legacy",
      this.#parentElement,
      this.#settingsButton,
      true,
      () => this.#openHubLegacy(),
      undefined,
      "appMenu-floorp-hub-button",
      "hub.menu.title",
    );
    this.#legacyRendered = true;
  }

  #mount(
    key: string,
    parent: Element,
    settings: Element,
    enabled: boolean,
    onCommand: () => void,
    owner: AppMenuOwner | undefined,
    buttonId: string,
    localizationKey: string,
  ): void {
    this.#removeOwned(key, "replacement");
    const trigger = this.#window.document.getElementById(
      "PanelUI-menu-button",
    );
    const focusContext: HubPanelMenuFocusContext | undefined = trigger
      ? {
        trigger,
        settings,
        isMenuOpen: () => trigger.getAttribute("open") === "true",
        isForeground: () => this.#window.document.hasFocus(),
      }
      : undefined;
    let view: OwnedHubPanelMenuView;
    try {
      view = mountOwnedHubPanelMenuView({
        parent,
        marker: settings.nextElementSibling,
        buttonId,
        localizationKey,
        enabled,
        owner,
        onCommand,
        readLabel: (key, fallback) =>
          i18next.t(key, { defaultValue: fallback }),
        observeTranslations: (update) => addI18nObserver(() => update()),
        reportFailure: (failureOwner, error) =>
          this.#reportFailure(failureOwner, error),
        focusContext,
      });
    } catch (error) {
      if (trigger) cancelHubMenuFocusClaim(trigger);
      throw error;
    }
    this.#owned.set(key, view);
  }

  #removeOwned(key: string, reason: AppMenuRemovalReason = "remove"): void {
    const errors: Error[] = [];
    try {
      this.#stopReadiness(key);
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
    const owned = this.#owned.get(key);
    if (owned) {
      try {
        owned.dispose(reason);
      } catch (error) {
        errors.push(error instanceof Error ? error : new Error(String(error)));
      } finally {
        this.#owned.delete(key);
        if (key === "legacy") this.#legacyRendered = false;
      }
    } else if (reason === "remove") {
      const trigger = this.#window.document.getElementById(
        "PanelUI-menu-button",
      );
      if (trigger) cancelHubMenuFocusClaim(trigger);
    }
    if (errors.length) {
      const failure = new AggregateError(
        errors,
        "HUB menu view cleanup failed",
      );
      console.error("[HubPanelMenu]", failure);
      throw failure;
    }
  }

  #openHubLegacy(): void {
    const browserWindow = this.#window as Window & {
      gBrowser: {
        addTab: (
          url: string,
          options: Parameters<typeof window.gBrowser.addTab>[1],
        ) => XULElement;
        selectedTab: XULElement;
      };
      PanelUI?: { hide?(): void };
    };
    browserWindow.gBrowser.selectedTab = browserWindow.gBrowser.addTab(
      "about:hub",
      {
        relatedToCurrent: true,
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      } as Parameters<typeof window.gBrowser.addTab>[1],
    );
    browserWindow.PanelUI?.hide?.();
  }

  #renderKey(owner: AppMenuRenderRequest["owner"]): string {
    return `${owner.hostEpoch}:${owner.generation}`;
  }

  #reportFailure(owner: AppMenuOwner, error: Error): boolean {
    if (this.#disposed) return false;
    if (!this.#connection) {
      this.#pendingFailures.push({ owner, error });
      return false;
    }
    return this.#connection.reportFailure(owner, error);
  }

  #dispose(): void {
    if (this.#disposed) return;
    this.#disposed = true;
    const errors: Error[] = [];
    try {
      this.#connection?.dispose();
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
    this.#connection = null;
    this.#pendingFailures.length = 0;
    try {
      this.#triggerObserver?.disconnect();
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
    try {
      this.#discoveryObserver?.disconnect();
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
    for (const stop of this.#readinessObservers.values()) {
      try {
        stop();
      } catch (error) {
        errors.push(error instanceof Error ? error : new Error(String(error)));
      }
    }
    this.#readinessObservers.clear();
    for (const key of [...this.#owned.keys()]) {
      try {
        this.#removeOwned(key);
      } catch (error) {
        errors.push(error instanceof Error ? error : new Error(String(error)));
      }
    }
    if (errors.length) {
      const failure = new AggregateError(
        errors,
        "HUB menu component cleanup failed",
      );
      console.error("[HubPanelMenu]", failure);
      throw failure;
    }
  }

  get #parentElement(): Element | null {
    return this.#window.document.querySelector(
      "#appMenu-mainView > .panel-subview-body",
    );
  }

  get #settingsButton(): Element | null {
    return this.#window.document.getElementById("appMenu-settings-button");
  }
}
