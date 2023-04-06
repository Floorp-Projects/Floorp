/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** This file handles registering the Storybook addon */

// eslint-disable-next-line no-unused-vars
import React from "react";
import { addons, types } from "@storybook/addons";
import { ADDON_ID, PANEL_ID, TOOL_ID } from "../constants.mjs";
import { PseudoLocalizationButton } from "../PseudoLocalizationButton.mjs";
// eslint-disable-next-line no-unused-vars
import { FluentPanel } from "../FluentPanel.mjs";

// Register the addon.
addons.register(ADDON_ID, api => {
  // Register the tool.
  addons.add(TOOL_ID, {
    type: types.TOOL,
    title: "Pseudo Localization",
    // Toolbar button doesn't show on the "Docs" tab.
    match: ({ viewMode }) => !!(viewMode && viewMode.match(/^story$/)),
    render: PseudoLocalizationButton,
  });

  addons.add(PANEL_ID, {
    title: "Fluent",
    //👇 Sets the type of UI element in Storybook
    type: types.PANEL,
    render: ({ active, key }) => (
      <FluentPanel active={active} api={api} key={key}></FluentPanel>
    ),
  });
});
