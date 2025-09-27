/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Portal } from "solid-js/web";
import { ModalBrowser } from "./browser.tsx";
import { isModalVisible } from "../data/data.ts";

type ModalProps = {
  targetParent: HTMLElement;
  onBackdropClick: (event: MouseEvent) => void;
};

export function Modal(props: ModalProps) {
  return (
    <Portal mount={props.targetParent}>
      <div
        id="modal-parent-container"
        data-visible={isModalVisible() ? "true" : undefined}
        onClick={(e) => props.onBackdropClick(e)}
      >
        <ModalBrowser />
      </div>
    </Portal>
  );
}
