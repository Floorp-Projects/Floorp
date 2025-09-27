/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { type Accessor, createSignal, type Setter } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";

export type ModalSize = {
  width?: number;
  height?: number;
};

const defaultModalSize: ModalSize = {
  width: 600,
  height: 800,
};

function createModalVisibility(): [Accessor<boolean>, Setter<boolean>] {
  return createSignal<boolean>(false);
}

function createModalSize(): [Accessor<ModalSize>, Setter<ModalSize>] {
  return createSignal<ModalSize>(defaultModalSize);
}

/** Modal visibility state */
export const [isModalVisible, setModalVisible] = createRootHMR(
  createModalVisibility,
  import.meta.hot,
);

/** Modal size state */
export const [modalSize, setModalSize] = createRootHMR(
  createModalSize,
  import.meta.hot,
);
