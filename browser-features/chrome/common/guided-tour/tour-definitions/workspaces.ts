// SPDX-License-Identifier: MPL-2.0

import type { TourDefinition } from "../types.ts";

export const workspacesTour: TourDefinition = {
  id: "workspaces",
  steps: [
    {
      selector: "#workspaces-toolbar-button",
      titleKey: "guidedTour.workspaces.step1.title",
      descriptionKey: "guidedTour.workspaces.step1.description",
      placement: "bottom",
    },
    {
      // ボタンを実際にクリックしてパネルを開いてもらう。
      // Next を押した場合は action がパネルを開く
      selector: "#workspaces-toolbar-button",
      titleKey: "guidedTour.workspaces.step2.title",
      descriptionKey: "guidedTour.workspaces.step2.description",
      placement: "bottom",
      passthrough: true,
      advanceOn: { event: "click", selector: "#workspaces-toolbar-button" },
    },
    {
      selector: "#workspacesToolbarButtonPanel",
      titleKey: "guidedTour.workspaces.step3.title",
      descriptionKey: "guidedTour.workspaces.step3.description",
      placement: "right",
      action: { type: "click", selector: "#workspaces-toolbar-button" },
      waitForSelector: "#workspacesToolbarButtonPanel",
    },
    {
      selector: "#workspacesToolbarButtonPanel",
      titleKey: "guidedTour.workspaces.step4.title",
      descriptionKey: "guidedTour.workspaces.step4.description",
      placement: "right",
      action: { type: "click", selector: "#workspaces-toolbar-button" },
      waitForSelector: "#workspacesToolbarButtonPanel",
    },
    {
      selector: null,
      titleKey: "guidedTour.workspaces.step5.title",
      descriptionKey: "guidedTour.workspaces.step5.description",
      placement: "center",
    },
    {
      selector: null,
      titleKey: "guidedTour.workspaces.step6.title",
      descriptionKey: "guidedTour.workspaces.step6.description",
      placement: "center",
    },
  ],
};
