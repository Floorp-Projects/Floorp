// SPDX-License-Identifier: MPL-2.0

import type { TourDefinition } from "../types.ts";
import { workspacesTour } from "./workspaces.ts";

const tourRegistry: Record<string, TourDefinition> = {
  [workspacesTour.id]: workspacesTour,
};

export function getTourById(tourId: string): TourDefinition | undefined {
  return Object.hasOwn(tourRegistry, tourId)
    ? tourRegistry[tourId]
    : undefined;
}

export function getAllTours(): TourDefinition[] {
  return Object.values(tourRegistry);
}
