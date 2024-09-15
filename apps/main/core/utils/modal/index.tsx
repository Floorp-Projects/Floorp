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
  name?: string;
  ContentElement: () => JSX.Element;
}) {
  return (
    <Portal mount={targetParent}>
      <div class="modal-overlay">
        <div class="modal-content">
          <button type="button" class="modal-close" onClick={props.onClose}>
            ×
          </button>
          <div class="modal-header">{props.name ?? "Modal Name"}</div>
          <div class="modal-body">{props.ContentElement()}</div>
        </div>
      </div>
    </Portal>
  );
}
