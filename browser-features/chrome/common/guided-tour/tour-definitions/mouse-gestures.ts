/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TourDefinition } from "../types.ts";

export const mouseGesturesTour: TourDefinition = {
  id: "mouseGestures",
  steps: [
    {
      selector: null,
      titleKey: "guidedTour.mouseGestures.step1.title",
      descriptionKey: "guidedTour.mouseGestures.step1.description",
      tooltipPlacement: "center",
    },
    {
      selector: "#tabbrowser-tabpanels",
      titleKey: "guidedTour.mouseGestures.step2.title",
      descriptionKey: "guidedTour.mouseGestures.step2.description",
      tooltipPlacement: "center",
      passthrough: true,
    },
    {
      selector: "#tabbrowser-tabpanels",
      titleKey: "guidedTour.mouseGestures.step3.title",
      descriptionKey: "guidedTour.mouseGestures.step3.description",
      tooltipPlacement: "center",
      passthrough: true,
    },
    {
      selector: null,
      titleKey: "guidedTour.mouseGestures.step4.title",
      descriptionKey: "guidedTour.mouseGestures.step4.description",
      tooltipPlacement: "center",
    },
  ],
};
