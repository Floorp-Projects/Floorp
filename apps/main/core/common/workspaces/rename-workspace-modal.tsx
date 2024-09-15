/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, Show } from "solid-js";
import { render } from "@nora/solid-xul";
import { ShareModal } from "@core/utils/modal";

export const [workspaceModalState, setWorkspaceModalState] = createSignal({
  show: true,
  targetWokspace: "",
});

export class WorkspaceRenameModal {
  private static instance: WorkspaceRenameModal;
  public static getInstance() {
    if (!WorkspaceRenameModal.instance) {
      WorkspaceRenameModal.instance = new WorkspaceRenameModal();
    }
    return WorkspaceRenameModal.instance;
  }

  private customModalStyle = {
    display: "none",
    position: "fixed",
    zIndex: 1,
    left: 0,
    top: 0,
    width: "100%",
    height: "100%",
    overflow: "auto",
    backgroundColor: "rgba(0,0,0,0.4)",
  };

  private ContentElement() {
    return <div>Workspace Modal</div>;
  }

  private Modal() {
    return (
      <ShareModal
        name="Rename Workspace"
        ContentElement={this.ContentElement}
        onClose={() =>
          setWorkspaceModalState({ show: false, targetWokspace: "" })
        }
      />
    );
  }

  private WorkspaceRenameModal() {
    return <Show when={workspaceModalState().show}>{this.Modal()}</Show>;
  }

  constructor() {
    render(
      () => this.WorkspaceRenameModal(),
      document?.getElementById("fullscreen-and-pointerlock-wrapper"),
      {
        hotCtx: import.meta.hot,
      },
    );
  }
}
