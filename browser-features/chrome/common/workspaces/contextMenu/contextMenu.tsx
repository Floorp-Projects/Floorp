/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID } from "../utils/type";
import type { WorkspacesService } from "../workspacesService";
import i18next from "i18next";
import { createSignal, Show } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

// Check if workspace archive experiment is enabled
// Simply checks if the user is assigned to the experiment (rollout-based)
const isArchiveFeatureEnabled = (): boolean => {
  try {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    const variant = Experiments.getVariant("workspace_archive");

    // If variant is assigned (not null), the feature is enabled
    // Rollout percentage controls what portion of users get the feature
    return variant !== null;
  } catch (error) {
    console.error("Failed to check workspace_archive experiment:", error);
    // If experiments system fails, default to disabled for safety
    return false;
  }
};

const translationKeys = {
  moveUp: "workspaces.context-menu.move-up",
  moveDown: "workspaces.context-menu.move-down",
  delete: "workspaces.context-menu.delete",
  manage: "workspaces.context-menu.manage",
  archive: "workspaces.context-menu.archive",
};

const translate = (
  key: string,
  options?: Record<string, unknown>,
): string => i18next.t(key, undefined, options) as string;

const getTranslations = () => ({
  moveUp: translate(translationKeys.moveUp),
  moveDown: translate(translationKeys.moveDown),
  delete: translate(translationKeys.delete),
  manage: translate(translationKeys.manage),
  archive: translate(translationKeys.archive),
});

export function ContextMenu(props: {
  disableBefore: boolean;
  disableAfter: boolean;
  contextWorkspaceId: TWorkspaceID;
  ctx: WorkspacesService;
}) {
  const [texts, setTexts] = createSignal(getTranslations());
  const archiveEnabled = isArchiveFeatureEnabled();

  addI18nObserver(() => {
    setTexts(getTranslations());
  });

  return (
    <>
      <xul:menuitem
        label={texts().moveUp}
        disabled={props.disableBefore}
        onCommand={() => props.ctx.reorderWorkspaceUp(props.contextWorkspaceId)}
      />
      <xul:menuitem
        label={texts().moveDown}
        disabled={props.disableAfter}
        onCommand={() =>
          props.ctx.reorderWorkspaceDown(props.contextWorkspaceId)}
      />
      <xul:menuseparator class="workspaces-context-menu-separator" />
      <xul:menuitem
        label={texts().delete}
        onCommand={() => props.ctx.deleteWorkspace(props.contextWorkspaceId)}
      />
      <xul:menuitem
        label={texts().manage}
        onCommand={() =>
          props.ctx.manageWorkspaceFromDialog(props.contextWorkspaceId)}
      />
      <Show when={archiveEnabled}>
        <xul:menuseparator class="workspaces-context-menu-separator" />
        <xul:menuitem
          label={texts().archive}
          onCommand={async () => {
            await props.ctx.archiveWorkspace(props.contextWorkspaceId);
          }}
        />
      </Show>
    </>
  );
}
