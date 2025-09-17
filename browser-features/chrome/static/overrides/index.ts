// SPDX-License-Identifier: MPL-2.0


import { Overrides } from "./overrides.js";

// THIS CANNOT BE HOT RELOADED
// TODO: REMOVE ALL CREATE_ROOT_HMR

export function init() {
  Overrides.getInstance();
}
