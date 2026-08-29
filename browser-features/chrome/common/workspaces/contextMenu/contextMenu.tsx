/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID } from "../utils/type";
import type { WorkspacesService } from "../workspacesService";
import i18next from "i18next";
import { createSignal } from "solid-js";
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

export function ContextMenu(props: {
  disableBefore: boolean;
  disableAfter: boolean;
  contextWorkspaceId: TWorkspaceID;
  ctx: WorkspacesService;
}) {
  const [texts, setTexts] = createSignal(getTranslations());

  addI18nObserver(() => {
    setTexts(getTranslations());
  });

  return (
    <>
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.move-up"
        label={texts().moveUp}
        disabled={props.disableBefore}
        onCommand={() => props.ctx.reorderWorkspaceUp(props.contextWorkspaceId)}
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.move-down"
        label={texts().moveDown}
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
        label={texts().delete}
        onCommand={() => props.ctx.deleteWorkspace(props.contextWorkspaceId)}
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.manage"
        label={texts().manage}
        onCommand={() =>
          props.ctx.manageWorkspaceFromDialog(props.contextWorkspaceId)}
      />
      <xul:menuseparator
        data-floorp-context-menu-key="floorp.workspaces.separator-archive"
        class="workspaces-context-menu-separator"
      />
      <xul:menuitem
        data-floorp-context-menu-key="floorp.workspaces.archive"
        label={texts().archive}
        onCommand={async () => {
          await props.ctx.archiveWorkspace(props.contextWorkspaceId);
        }}
      />
    </>
  );
}
