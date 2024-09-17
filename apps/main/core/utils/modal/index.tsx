import type { JSX } from "solid-js";
import { Portal } from "solid-js/web";
import modalStyle from "./styles.css?inline";
import { render } from "@nora/solid-xul";
import { setWorkspaceModalState } from "@core/common/workspaces/workspace-modal";

const targetParent = document?.body as HTMLElement;

render(() => <style>{modalStyle}</style>, document?.head, {
  hotCtx: import.meta.hot,
  marker: document?.head?.lastChild as Element,
});

type FormData = {
  name: string;
};

export function ShareModal(props: {
  onClose: () => void;
  onSave: (formControls: { id: string; value: string }[]) => void;
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
              onClick={() => {
                console.log("save-modal");
                const forms =
                  document?.getElementsByClassName("form-control") || [];
                const result = Array.from(forms).map((e) => {
                  const element = e as HTMLInputElement;
                  if (!element.id || !element.value) {
                    throw new Error(
                      `Invalid Modal Form Control: "Id" and "Value" are required for all form elements!`,
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
      </div>
    </Portal>
  );
}
