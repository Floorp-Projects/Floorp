// SPDX-License-Identifier: MPL-2.0

import { ContextMenuController } from "./controller.ts";
import type {
  ContextMenuExperiments,
  ContextMenuRuntimeController,
  ContextMenuRuntimeOptions,
  ContextMenuRuntimePreferences,
} from "./runtime-types.ts";
import { CONTEXT_MENU_ENABLED_PREF } from "./types.ts";

export const CONTEXT_MENU_EXPERIMENT_ID = "context_menu_customization";
const POLICY_PREF = "floorp.experiments.participationPolicy";
const LAST_POLICY_PREF = "floorp.experiments.lastPolicy";

/** Owns only the availability watchers until Flasco permits the customizer. */
export class ContextMenuRuntime {
  readonly #window: Window;
  readonly #preferences: ContextMenuRuntimePreferences;
  readonly #createController: () => ContextMenuRuntimeController;
  #experiments: ContextMenuExperiments | undefined;
  #controller: ContextMenuRuntimeController | null = null;
  #unsubscribe: (() => void) | null = null;
  #timer: number | null = null;
  #started = false;

  readonly #onPreferenceChange: nsIObserver = () => this.refresh();

  constructor(options: ContextMenuRuntimeOptions) {
    this.#window = options.window;
    this.#preferences = options.preferences ?? Services.prefs;
    this.#experiments = options.experiments;
    // Do not construct the registry, config store, or catalog outside the gate.
    this.#createController = options.createController ??
      (() => new ContextMenuController({ window: this.#window }));
  }

  start(): void {
    if (this.#started) return;
    this.#started = true;
    try {
      if (!this.#experiments) {
        const { Experiments } = ChromeUtils.importESModule(
          "resource://noraneko/modules/experiments/Experiments.sys.mjs",
        ) as { Experiments: ContextMenuExperiments };
        this.#experiments = Experiments;
      }
      this.#unsubscribe = this.#experiments.subscribe(() => this.refresh());
      this.#preferences.addObserver(POLICY_PREF, this.#onPreferenceChange);
      this.#preferences.addObserver(
        CONTEXT_MENU_ENABLED_PREF,
        this.#onPreferenceChange,
      );
      this.refresh();
    } catch (error) {
      console.error("[ContextMenuCustomizer] Failed to watch Flasco", error);
      this.destroy();
    }
  }

  private refresh(): void {
    if (!this.#started) return;
    if (this.#timer !== null) this.#window.clearTimeout(this.#timer);
    this.#timer = null;
    let enabled = false;
    try {
      const policy = this.#preferences.getStringPref(POLICY_PREF, "default");
      if (
        this.#experiments?.manifestAvailable &&
        !this.#experiments.initializing && policy !== "never" &&
        policy === this.#preferences.getStringPref(LAST_POLICY_PREF, "")
      ) {
        let experiment = this.#experiments.getAllExperiments().find(
          (entry) => entry.id === CONTEXT_MENU_EXPERIMENT_ID,
        );
        if (
          this.#preferences.getBoolPref(CONTEXT_MENU_ENABLED_PREF, true) &&
          experiment?.isActive && experiment.currentVariantId === null &&
          this.#experiments.ensureActiveAssignment(CONTEXT_MENU_EXPERIMENT_ID)
        ) {
          experiment = this.#experiments.getAllExperiments().find(
            (entry) => entry.id === CONTEXT_MENU_EXPERIMENT_ID,
          );
        }
        enabled =
          this.#preferences.getBoolPref(CONTEXT_MENU_ENABLED_PREF, true) &&
          experiment?.isActive === true &&
          experiment.currentVariantId === "enabled" &&
          (experiment.enrollmentStatus === "enrolled" ||
            experiment.enrollmentStatus === "force_enrolled");

        // Match the client's UTC date-only/ISO start and end semantics. A
        // long-lived window must also stop at expiry without another UI action.
        const now = Date.now();
        const nextBoundary = [experiment?.start, experiment?.end]
          .filter((value): value is string => Boolean(value))
          .map((value) => Date.parse(value))
          .filter((value) => value > now)
          .sort((left, right) => left - right)[0];
        if (nextBoundary !== undefined) {
          // Assignment recovery can synchronously notify and schedule a timer
          // in a nested refresh. Replace that timer rather than orphaning it.
          if (this.#timer !== null) this.#window.clearTimeout(this.#timer);
          this.#timer = this.#window.setTimeout(
            () => this.refresh(),
            Math.min(nextBoundary - now + 1, 0x7fffffff),
          );
        }
      }
    } catch (error) {
      console.error("[ContextMenuCustomizer] Failed to read Flasco", error);
    }

    if (!enabled) {
      this.stopController();
    } else if (!this.#controller) {
      try {
        this.#controller = this.#createController();
        this.#controller.attach();
      } catch (error) {
        this.stopController();
        console.error(
          "[ContextMenuCustomizer] Failed to start customizer",
          error,
        );
      }
    }
  }

  private stopController(): void {
    const controller = this.#controller;
    this.#controller = null;
    controller?.destroy();
  }

  destroy(): void {
    if (!this.#started) return;
    this.#started = false;
    this.#unsubscribe?.();
    this.#unsubscribe = null;
    for (const name of [POLICY_PREF, CONTEXT_MENU_ENABLED_PREF]) {
      try {
        this.#preferences.removeObserver(name, this.#onPreferenceChange);
      } catch {
        // Startup may have failed before this observer was registered.
      }
    }
    if (this.#timer !== null) this.#window.clearTimeout(this.#timer);
    this.#timer = null;
    this.stopController();
  }
}
