// SPDX-License-Identifier: MPL-2.0

import type { NRExperimemmtParentFunctions } from "../../../../modules/common/defines.ts";

export type ContextMenuAvailability = "loading" | "available" | "unavailable";

export interface ContextMenuAvailabilityState {
  pathname: string;
  availability: ContextMenuAvailability;
}

export interface ExperimentsModule {
  Experiments:
    & Omit<
      NRExperimemmtParentFunctions,
      "clearExperimentCache" | "reinitializeExperiments"
    >
    & {
      clearCache(): unknown;
      init(): Promise<void>;
    };
}
