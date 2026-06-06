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

    const destroyOverlay = (): void => {
      if (overlay) {
        overlay.destroy();
        overlay = null;
      }
    };

    const showStep = (tourId: string, stepIndex: number): void => {
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

      if (typeof stepIndex !== "number" || stepIndex < 0 || stepIndex >= steps.length) {
        destroyOverlay();
        clearPref();
        return;
      }

      const step = steps[stepIndex];
      const totalSteps = steps.length;

      // If the step has a waitForSelector, check if it exists
      if (step.waitForSelector) {
        const target = document.querySelector(step.waitForSelector);
        if (!target) {
          // Element not found, skip after timeout
          const timeout = step.waitTimeout ?? 2000;
          setTimeout(() => {
            // Guard: check if tour is still active before retrying
            const currentState = parsePref();
            if (!currentState || currentState.tourId !== tourId) {
              return;
            }
            const retryTarget = document.querySelector(
              step.waitForSelector!,
            );
            if (!retryTarget) {
              // Still not found, skip to next step
              showStep(tourId, stepIndex + 1);
            } else {
              showStep(tourId, stepIndex);
            }
          }, timeout);
          return;
        }
      }

      // Execute action before showing the step
      if (step.action) {
        executeAction(step.action);
      }

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
              showStep(newState.tourId, newState.currentStep);
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
              showStep(newState.tourId, newState.currentStep);
            }
          },
          onSkip: () => {
            destroyOverlay();
            setCurrentTour(null);
            clearPref();
          },
        });
        overlay.create();
      }

      // Update overlay with translated text
      const translatedStep = {
        ...step,
        titleKey: getTranslatedText(step.titleKey),
        descriptionKey: getTranslatedText(step.descriptionKey),
      };
      overlay.updateStep(translatedStep, stepIndex, totalSteps);
    };

    const executeAction = (
      action: import("./types.ts").TourAction,
    ): void => {
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
              el.dispatchEvent(
                new MouseEvent("contextmenu", {
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
