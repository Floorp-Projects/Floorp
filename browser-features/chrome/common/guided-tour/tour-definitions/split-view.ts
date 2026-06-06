/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TourDefinition } from "../types.ts";

export const splitViewTour: TourDefinition = {
  id: "splitView",
  steps: [
    {
      selector: "#split-view-button",
      titleKey: "guidedTour.splitView.step1.title",
      descriptionKey: "guidedTour.splitView.step1.description",
      tooltipPlacement: "bottom",
    },
    {
      selector: "#tabbrowser-tabpanels",
      titleKey: "guidedTour.splitView.step2.title",
      descriptionKey: "guidedTour.splitView.step2.description",
      tooltipPlacement: "center",
      action: {
        type: "rightClick",
        selector: ".tabbrowser-tab[selected]",
      },
    },
    {
      selector: "#floorp_openInSplitView",
      titleKey: "guidedTour.splitView.step3.title",
      descriptionKey: "guidedTour.splitView.step3.description",
      tooltipPlacement: "right",
      waitForSelector: "#floorp_openInSplitView",
    },
    {
      selector: "#split-view-button",
      titleKey: "guidedTour.splitView.step4.title",
      descriptionKey: "guidedTour.splitView.step4.description",
      tooltipPlacement: "bottom",
    },
  ],
};
