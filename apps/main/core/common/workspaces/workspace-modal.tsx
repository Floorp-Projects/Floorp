/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, For, Show } from "solid-js";
import { render } from "@nora/solid-xul";
import { ShareModal } from "@core/utils/modal";
import { WorkspaceIcons } from "./utils/workspace-icons";
import type { workspace } from "./utils/type";

export const [workspaceModalState, setWorkspaceModalState] = createSignal<{
  show: boolean;
  targetWokspace: workspace | null;
}>({ show: true, targetWokspace: null });

export class WorkspaceManageModal {
  private static instance: WorkspaceManageModal;
  public static getInstance() {
    if (!WorkspaceManageModal.instance) {
      WorkspaceManageModal.instance = new WorkspaceManageModal();
    }
    return WorkspaceManageModal.instance;
  }

  private ContentElement(workspace: workspace | null) {
    const gWorkspaceIcons = WorkspaceIcons.getInstance();
    return (
      <>
        <label>名前</label>
        <input
          type="text"
          id="name"
          class="form-control"
          placeholder="名前を入力"
        />
        <label>アイコン</label>
        <select id="containerName">
          <For each={gWorkspaceIcons.workspaceIconsArray}>
            {(icon) => <option label={icon} value={icon} />}
          </For>
        </select>
        <label>メッセージ</label>
        <textarea
          id="message"
          class="form-control"
          rows="3"
          placeholder="メッセージを入力"
        />
      </>
    );
  }

  private Modal() {
    return (
      <ShareModal
        name="Manage Workspace"
        ContentElement={() =>
          this.ContentElement(workspaceModalState().targetWokspace)
        }
        onClose={() =>
          setWorkspaceModalState({ show: false, targetWokspace: null })
        }
        onSave={() => {}}
      />
    );
  }

  private WorkspaceManageModal() {
    return <Show when={workspaceModalState().show}>{this.Modal()}</Show>;
  }

  constructor() {
    render(
      () => this.WorkspaceManageModal(),
      document?.getElementById("fullscreen-and-pointerlock-wrapper"),
      {
        hotCtx: import.meta.hot,
      },
    );
  }
}
