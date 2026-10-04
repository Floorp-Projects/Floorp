/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createPortal } from "preact/compat";
import { ModalBrowser } from "./browser.tsx";
import { isModalVisible } from "../data/data.ts";

type ModalProps = {
  targetParent: HTMLElement;
  onBackdropClick: (event: MouseEvent) => void;
};

export function Modal(props: ModalProps) {
  return (
    <>
      {createPortal(
        <div
          id="modal-parent-container"
          data-visible={isModalVisible.value ? "true" : undefined}
          onClick={(e) => props.onBackdropClick(e as unknown as MouseEvent)}
        >
          <ModalBrowser />
        </div>,
        props.targetParent,
      )}
    </>
  );
}
