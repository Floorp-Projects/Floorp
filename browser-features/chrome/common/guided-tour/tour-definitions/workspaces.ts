/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TourDefinition } from "../types.ts";

export const workspacesTour: TourDefinition = {
  id: "workspaces",
  steps: [
    {
      selector: "#workspaces-toolbar-button",
      titleKey: "guidedTour.workspaces.step1.title",
      descriptionKey: "guidedTour.workspaces.step1.description",
      tooltipPlacement: "bottom",
    },
    {
      selector: "#workspacesToolbarButtonPanel",
      titleKey: "guidedTour.workspaces.step2.title",
      descriptionKey: "guidedTour.workspaces.step2.description",
      tooltipPlacement: "bottom",
      action: { type: "click", selector: "#workspaces-toolbar-button" },
      waitForSelector: "#workspacesToolbarButtonPanel",
    },
    {
      selector: "#workspacesToolbarButtonPanel",
      titleKey: "guidedTour.workspaces.step3.title",
      descriptionKey: "guidedTour.workspaces.step3.description",
      tooltipPlacement: "bottom",
      waitForSelector: "#workspacesToolbarButtonPanel",
    },
    {
      selector: "#workspaces-toolbar-button",
      titleKey: "guidedTour.workspaces.step4.title",
      descriptionKey: "guidedTour.workspaces.step4.description",
      tooltipPlacement: "right",
    },
    {
      selector: null,
      titleKey: "guidedTour.workspaces.step5.title",
      descriptionKey: "guidedTour.workspaces.step5.description",
      tooltipPlacement: "center",
    },
    {
      selector: "#workspaces-panel-sidebar-section",
      titleKey: "guidedTour.workspaces.step6.title",
      descriptionKey: "guidedTour.workspaces.step6.description",
      tooltipPlacement: "right",
    },
  ],
};
