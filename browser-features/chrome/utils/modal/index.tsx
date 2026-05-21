// SPDX-License-Identifier: MPL-2.0

import type { ComponentChild, VNode } from "preact";
import { createPortal } from "preact/compat";
import modalStyle from "./styles.css?inline";
import { render } from "@nora/preact-xul";
import { createRootHMR } from "@nora/preact-xul/lifetime";

const targetParent = document?.getElementById("appcontent") as HTMLElement;

createRootHMR(
  () => {
    if (document?.head) {
      const styleWrapper = document.createElement("div");
      document.head.appendChild(styleWrapper);
      render(() => <style>{modalStyle}</style>, styleWrapper);
    }
  },
  import.meta.hot,
);

export function ShareModal(props: {
  onClose: () => void;
  onSave: (formControls: { id: string; value: string }[]) => void;
  ContentElement: () => ComponentChild;
  StyleElement?: () => VNode;
  name?: string;
}) {
  return (
    <>
      {createPortal(
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
                onClick={() => {
                  const forms =
                    document?.getElementsByClassName("form-control") || [];
                  const result = Array.from(forms).map((e) => {
                    const element = e as HTMLInputElement;
                    if (!element.id || !element.value) {
                      throw new Error(
                        `Invalid Modal Form Control: "Id" and "Value" are required for all form elements! Occured element: ${element.id}, ${element.value}`,
                      );
                    }
                    return {
                      id: element.id as string,
                      value: element.value as string,
                    };
                  });
                  props.onSave(result);
                }}
              >
                保存
              </button>
            </div>
          </div>
        </div>,
        targetParent,
      )}
    </>
  );
}
