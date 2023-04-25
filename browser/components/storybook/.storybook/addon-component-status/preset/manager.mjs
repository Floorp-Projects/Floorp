/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** This file handles registering the Storybook addon */

// eslint-disable-next-line no-unused-vars
import React from "react";
import { addons, types } from "@storybook/addons";
import { ADDON_ID, TOOL_ID } from "../constants.mjs";
import { StatusIndicator } from "../StatusIndicator.mjs";

addons.register(ADDON_ID, () => {
  addons.add(TOOL_ID, {
    type: types.TOOL,
    title: "Pseudo Localization",
    render: StatusIndicator,
  });
});
