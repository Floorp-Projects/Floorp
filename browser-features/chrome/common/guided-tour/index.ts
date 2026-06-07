/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { createSignal, onCleanup } from "solid-js";
import i18next from "i18next";
import { TOUR_PREF, type TourState } from "./types.ts";
import { getTourById } from "./tour-definitions/index.ts";
import { GuidedTourOverlay } from "./overlay.ts";

@noraComponent(import.meta.hot)
export default class GuidedTour extends NoraComponentBase {
  init(): void {
    const [currentTour, setCurrentTour] = createSignal<TourState | null>(null);
    let overlay: GuidedTourOverlay | null = null;

    const parsePref = (): TourState | null => {
      try {
        const raw = Services.prefs.getStringPref(TOUR_PREF, "");
        if (!raw) return null;
        const parsed = JSON.parse(raw);
        // Normalize: accept both { tourId, currentStep } and legacy { tourId, step }
        const state: TourState = {
          tourId: parsed.tourId,
          currentStep: parsed.currentStep ?? parsed.step ?? 0,
        };
        return state;
      } catch {
        return null;
      }
    };

    const clearPref = (): void => {
      try {
        Services.prefs.setStringPref(TOUR_PREF, "");
      } catch (e) {
        console.error("[GuidedTour]", e, "clearing pref");
      }
    };

    const setPref = (state: TourState): void => {
      try {
        Services.prefs.setStringPref(TOUR_PREF, JSON.stringify(state));
      } catch (e) {
        console.error("[GuidedTour]", e, "setting pref");
      }
    };

    let healthCheckTimer: ReturnType<typeof setInterval> | null = null;

    const destroyOverlay = (): void => {
      if (healthCheckTimer) {
        clearInterval(healthCheckTimer);
        healthCheckTimer = null;
      }
      if (overlay) {
        overlay.destroy();
        overlay = null;
      }
    };

    let pollingStepIndex = -1; // Guard: step currently being polled

    const showStep = (tourId: string, stepIndex: number): void => {
      // Skip duplicate calls for a step that's already being polled
      if (pollingStepIndex === stepIndex) {
        console.log("[GuidedTour:showStep] skipping duplicate for polling step", stepIndex);
        return;
      }

      console.log("[GuidedTour:showStep]", { tourId, stepIndex });

      const tourDef = getTourById(tourId);
      if (!tourDef) {
        console.warn("[GuidedTour] unknown tour:", tourId);
        destroyOverlay();
        clearPref();
        return;
      }

      const steps = tourDef.steps;
      if (!steps || !Array.isArray(steps)) {
        console.error("[GuidedTour] tourDef.steps is invalid:", steps);
        destroyOverlay();
        clearPref();
        return;
      }

      if (
        typeof stepIndex !== "number" ||
        stepIndex < 0 ||
        stepIndex >= steps.length
      ) {
        console.log("[GuidedTour:showStep] step out of range, ending tour");
        destroyOverlay();
        clearPref();
        return;
      }

      const step = steps[stepIndex];

      console.log("[GuidedTour:showStep] step data", {
        stepIndex,
        selector: step.selector,
        placement: step.tooltipPlacement,
        waitForSelector: step.waitForSelector,
        hasAction: !!step.action,
      });

      // If the step has a waitForSelector, check if it exists AND is visible
      if (step.waitForSelector) {
        const isVisible = (el: Element): boolean => {
          // XUL panels/menupopups use state property, not CSS layout
          const xulState = (el as Record<string, unknown>).state;
          if (typeof xulState === "string") {
            return xulState === "open" || xulState === "showing";
          }
          // Fallback: check CSS dimensions
          const rect = el.getBoundingClientRect();
          return rect.width > 0 && rect.height > 0;
        };
        const target = document.querySelector(step.waitForSelector);
        const targetVisible = target ? isVisible(target) : false;

        console.log("[GuidedTour:showStep] waitForSelector check:", step.waitForSelector, {
          found: !!target,
          visible: targetVisible,
          state: target ? (target as Record<string, unknown>).state : undefined,
        });

        // Only execute action if target is NOT already visible
        // (avoids toggling off an already-open panel)
        if (!targetVisible && step.action) {
          console.log("[GuidedTour:showStep] executing action:", step.action);
          executeAction(step.action);
        }

        if (!targetVisible) {
          // Poll until it appears or timeout
          const timeout = step.waitTimeout ?? 2000;
          const deadline = Date.now() + timeout;
          pollingStepIndex = stepIndex;
          const poll = (): void => {
            const currentState = parsePref();
            if (!currentState || currentState.tourId !== tourId) {
              pollingStepIndex = -1;
              return;
            }

            const el = document.querySelector(step.waitForSelector!);
            if (el && isVisible(el)) {
              // Now visible — proceed with overlay (do NOT re-call showStep)
              pollingStepIndex = -1;
              showStepOverlay(tourId, stepIndex, step, steps.length);
            } else if (Date.now() < deadline) {
              setTimeout(poll, 100);
            } else {
              console.log("[GuidedTour:showStep] waitForSelector timed out, skipping step");
              pollingStepIndex = -1;
              showStep(tourId, stepIndex + 1);
            }
          };
          setTimeout(poll, 100);
          return;
        }
      } else {
        // No waitForSelector — execute action unconditionally
        if (step.action) {
          console.log("[GuidedTour:showStep] executing action:", step.action);
          executeAction(step.action);
        }
      }

      showStepOverlay(tourId, stepIndex, step, steps.length);
    };

    const showStepOverlay = (
      tourId: string,
      stepIndex: number,
      step: import("./types.ts").TourStep,
      totalSteps: number,
    ): void => {
      // Create or reuse overlay
      if (!overlay) {
        overlay = new GuidedTourOverlay(window, {
          onNext: () => {
            const state = currentTour();
            if (state) {
              const newState = {
                ...state,
                currentStep: state.currentStep + 1,
              };
              setCurrentTour(newState);
              setPref(newState);
              // showStep will be called by the pref observer
            }
          },
          onPrev: () => {
            const state = currentTour();
            if (state && state.currentStep > 0) {
              const newState = {
                ...state,
                currentStep: state.currentStep - 1,
              };
              setCurrentTour(newState);
              setPref(newState);
              // showStep will be called by the pref observer
            }
          },
          onSkip: () => {
            destroyOverlay();
            setCurrentTour(null);
            clearPref();
          },
        });
        overlay.create();
        // Start periodic health check
        if (healthCheckTimer) clearInterval(healthCheckTimer);
        healthCheckTimer = setInterval(() => {
          if (overlay) overlay.checkPanelState();
        }, 500);
      }

      // Update overlay with translated text
      const translatedStep = {
        ...step,
        titleKey: getTranslatedText(step.titleKey),
        descriptionKey: getTranslatedText(step.descriptionKey),
      };
      console.log("[GuidedTour:showStepOverlay] updating overlay", {
        stepIndex,
        totalSteps,
        translatedTitle: translatedStep.titleKey,
      });
      overlay.updateStep(translatedStep, stepIndex, totalSteps);
    };

    const executeAction = (action: import("./types.ts").TourAction): void => {
      try {
        switch (action.type) {
          case "click": {
            const el = document.querySelector(action.selector);
            if (el) (el as HTMLElement).click();
            break;
          }
          case "rightClick": {
            const el = document.querySelector(action.selector);
            if (el) {
              // Use PointerEvent — Firefox 151+ asserts that trusted pointer
              // event messages must use the PointerEvent constructor
              // (MouseEvent.cpp:161).
              el.dispatchEvent(
                new PointerEvent("contextmenu", {
                  bubbles: true,
                  button: 2,
                }),
              );
            }
            break;
          }
          case "scrollIntoView": {
            const el = document.querySelector(action.selector);
            if (el) el.scrollIntoView({ behavior: "smooth", block: "center" });
            break;
          }
        }
      } catch (e) {
        console.error("[GuidedTour]", e, "executing action");
      }
    };

    // Watch the pref
    const observer = (): void => {
      const state = parsePref();
      setCurrentTour(state);

      if (!state) {
        destroyOverlay();
        return;
      }

      // Check panel health when pref changes
      if (overlay) {
        overlay.checkPanelState();
      }

      showStep(state.tourId, state.currentStep);
    };

    // Register pref observer
    Services.prefs.addObserver(TOUR_PREF, observer);
    onCleanup(() => {
      Services.prefs.removeObserver(TOUR_PREF, observer);
      destroyOverlay();
    });

    // Check if a tour is already active on init
    const initialState = parsePref();
    if (initialState) {
      setCurrentTour(initialState);
      showStep(initialState.tourId, initialState.currentStep);
    }
  }
}

function getTranslatedText(key: string): string {
  try {
    return i18next.t(key);
  } catch {
    return key;
  }
}
