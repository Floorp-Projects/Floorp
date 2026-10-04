/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID } from "../utils/type";
import type { WorkspacesService } from "../workspacesService";
import i18next from "i18next";
import { signal } from "@preact/signals";
import { createRootHMR } from "@nora/preact-xul/lifetime";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

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
): string => i18next.t(key, options) as string;

const getTranslations = () => ({
  moveUp: translate(translationKeys.moveUp),
  moveDown: translate(translationKeys.moveDown),
  delete: translate(translationKeys.delete),
  manage: translate(translationKeys.manage),
  archive: translate(translationKeys.archive),
});

// Module-level signal: translations are shared across all ContextMenu instances.
const texts = signal(getTranslations());
createRootHMR(() =>
  addI18nObserver(() => {
    texts.value = getTranslations();
  }), import.meta.hot);

export function ContextMenu(props: {
  disableBefore: boolean;
  disableAfter: boolean;
  contextWorkspaceId: TWorkspaceID;
  ctx: WorkspacesService;
}) {
  return (
    <>
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.move-up"
        label={texts.value.moveUp}
        disabled={props.disableBefore}
        onCommand={() => props.ctx.reorderWorkspaceUp(props.contextWorkspaceId)}
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.move-down"
        label={texts.value.moveDown}
        disabled={props.disableAfter}
        onCommand={() =>
          props.ctx.reorderWorkspaceDown(props.contextWorkspaceId)}
      />
      <xul:menuseparator
        data-floorp-context-menu-key="floorp.workspaces.separator-delete"
        class="workspaces-context-menu-separator"
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.delete"
        label={texts.value.delete}
        onCommand={() => props.ctx.deleteWorkspace(props.contextWorkspaceId)}
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.manage"
        label={texts.value.manage}
        onCommand={() =>
          props.ctx.manageWorkspaceFromDialog(props.contextWorkspaceId)}
      />
      <xul:menuseparator
        data-floorp-context-menu-key="floorp.workspaces.separator-archive"
        class="workspaces-context-menu-separator"
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.archive"
        label={texts.value.archive}
        onCommand={async () => {
          await props.ctx.archiveWorkspace(props.contextWorkspaceId);
        }}
      />
    </>
  );
}
