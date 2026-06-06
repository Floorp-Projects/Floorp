/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TourDefinition } from "../types.ts";
import { workspacesTour } from "./workspaces.ts";
import { mouseGesturesTour } from "./mouse-gestures.ts";
import { splitViewTour } from "./split-view.ts";

const tourRegistry: Record<string, TourDefinition> = {
  workspaces: workspacesTour,
  mouseGestures: mouseGesturesTour,
  splitView: splitViewTour,
};

export function getTourById(tourId: string): TourDefinition | undefined {
  return tourRegistry[tourId];
}
