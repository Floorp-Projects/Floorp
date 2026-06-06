/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type TooltipPlacement = "top" | "bottom" | "left" | "right" | "center";

export type TourAction =
  | { type: "click"; selector: string }
  | { type: "rightClick"; selector: string }
  | { type: "scrollIntoView"; selector: string };

export interface TourStep {
  /** CSS selector for the target element (null = full-screen centered tooltip) */
  selector: string | null;

  /** i18n key for step title */
  titleKey: string;

  /** i18n key for step description */
  descriptionKey: string;

  /** Where to position the tooltip relative to the target */
  tooltipPlacement: TooltipPlacement;

  /** Optional: action to perform before showing this step */
  action?: TourAction;

  /** Optional: wait for this selector to appear before showing the step */
  waitForSelector?: string;

  /** Optional: max time in ms to wait for waitForSelector before skipping */
  waitTimeout?: number;

  /** Whether this step allows pointer events to pass through to the content area */
  passthrough?: boolean;
}

export interface TourDefinition {
  id: string;
  steps: TourStep[];
}

export interface TourState {
  tourId: string;
  currentStep: number;
}

export const TOUR_PREF = "floorp.guidedTour.active";
