/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import { createRootHMR } from "@nora/preact-xul/lifetime";

export type ModalSize = {
  width?: number;
  height?: number;
  maxWidth?: number;
  maxHeight?: number;
};

const defaultModalSize: ModalSize = {
  width: 600,
  height: 800,
};

/** Modal visibility state */
export const isModalVisible = createRootHMR(
  () => signal<boolean>(false),
  import.meta.hot,
);

/** Modal size state */
export const modalSize = createRootHMR(
  () => signal<ModalSize>(defaultModalSize),
  import.meta.hot,
);
