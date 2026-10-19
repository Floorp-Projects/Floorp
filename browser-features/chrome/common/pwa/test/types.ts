// SPDX-License-Identifier: MPL-2.0

import type { Signal } from "solid-js";
import type { TPwaConfig } from "../type.ts";

export interface PwaConfigTestInstance {
  enabled: Signal<boolean>;
  config: Signal<TPwaConfig>;
}

export interface PageActionTestInstance {
  disposed: boolean;
  onCheckPageHasManifest: () => Promise<void>;
}
