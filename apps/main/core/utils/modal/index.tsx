/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { JSX } from "solid-js";
import { Portal } from "solid-js/web";
import modalStyle from "./styles.css?inline";
import { render } from "@nora/solid-xul";

const targetParent = document?.body as HTMLElement;

render(() => <style>{modalStyle}</style>, document?.head, {
  hotCtx: import.meta.hot,
  marker: document?.head?.lastChild as Element,
});

export function ShareModal(props: {
  onClose: () => void;
  onSave: () => void;
  name?: string;
  ContentElement: () => JSX.Element;
}) {
  return (
    <Portal mount={targetParent}>
      <div class="modal-overlay" id="modal-overlay">
        <div class="modal">
          <div class="modal-header">{props.name}</div>
          <div class="modal-content">{props.ContentElement()}</div>
          <div class="modal-actions">
            <button
              class="modal-button"
              type="button"
              id="close-modal"
              onClick={props.onClose}
            >
              キャンセル
            </button>
            <button
              class="modal-button primary"
              type="button"
              id="save-modal"
              onClick={props.onSave}
            >
              保存
            </button>
          </div>
        </div>
      </div>
    </Portal>
  );
}
