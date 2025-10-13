/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  createEffect,
  createMemo,
  createSignal,
  For,
  onCleanup,
  Show,
} from "solid-js";
import Workspaces from "../index.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import { configStore, enabled } from "../data/config.ts";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";

const translationKeys = {
  createNew: "workspaces.popup.create-new",
  manage: "workspaces.popup.manage",
};

const getTranslations = () => ({
  createNew: i18next.t(translationKeys.createNew),
  manage: i18next.t(translationKeys.manage),
});

const DEFAULT_ICON = "chrome://branding/content/icon32.png";
const CONTEXT_MENU_ID = "workspaces-toolbar-item-context-menu";

type ChromeDocument = Document & { popupNode?: Element | null };

type WorkspacePanelButtonProps = {
  workspaceId: TWorkspaceID;
  ctx: WorkspacesService;
};

const WorkspacePanelButton = (props: WorkspacePanelButtonProps) => {
  const workspace = createMemo(() =>
    workspacesDataStore.data.get(props.workspaceId)
  );
  const iconUrl = createMemo(() =>
    props.ctx.iconCtx.getWorkspaceIconUrl(workspace()?.icon) ?? DEFAULT_ICON
  );
  const workspaceName = createMemo(() => workspace()?.name ?? "");
  const isSelected = () => selectedWorkspaceID() === props.workspaceId;

  const handleActivate = () => {
    if (!isSelected()) {
      props.ctx.changeWorkspace(props.workspaceId);
    }
  };

  const handleContextMenu = (event: MouseEvent) => {
    event.preventDefault();
    event.stopPropagation();

    const target = event.currentTarget as XULElement;
    (document as ChromeDocument).popupNode = target;

    const menu = document?.getElementById(CONTEXT_MENU_ID) as
      | XULPopupElement
      | null;

    menu?.openPopupAtScreen(event.screenX, event.screenY, true);
  };

  const handleKeyDown = (event: KeyboardEvent) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      handleActivate();
    }
  };

  return (
    <div class="panel-sidebar-button-wrapper workspace-panel-button-wrapper">
      <div
        id={`workspace-${props.workspaceId}`}
        class="workspace-panel panel-sidebar-panel"
        data-checked={isSelected() ? "true" : undefined}
        data-workspace-id={props.workspaceId}
        role="button"
        tabIndex={0}
        title={workspaceName()}
        aria-label={workspaceName()}
        onClick={handleActivate}
        onKeyDown={handleKeyDown}
        onContextMenu={handleContextMenu}
      >
        <img src={iconUrl()} width="16" height="16" alt="" />
      </div>
    </div>
  );
};

type ControlButtonProps = {
  id: string;
  icon: string;
  label: string;
  onActivate: () => void;
};

const ControlButton = (props: ControlButtonProps) => {
  const handleKeyDown = (event: KeyboardEvent) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      props.onActivate();
    }
  };

  return (
    <div class="panel-sidebar-button-wrapper workspace-panel-button-wrapper">
      <div
        id={props.id}
        class="workspace-panel-control panel-sidebar-panel"
        role="button"
        tabIndex={0}
        title={props.label}
        aria-label={props.label}
        onClick={props.onActivate}
        onKeyDown={handleKeyDown}
      >
        <img src={props.icon} width="16" height="16" alt="" />
      </div>
    </div>
  );
};

export function WorkspacesPanels(props: { ctx?: WorkspacesService } = {}) {
  const [texts, setTexts] = createSignal(getTranslations());
  const [ctx, setCtx] = createSignal<WorkspacesService | null>(
    props.ctx ?? Workspaces.getCtx(),
  );

  addI18nObserver(() => {
    setTexts(getTranslations());
  });

  const shouldShow = () => enabled() && Boolean(configStore.manageOnBms);

  createEffect(() => {
    if (props.ctx) {
      setCtx(props.ctx);
      return;
    }

    if (!shouldShow()) {
      setCtx(null);
      return;
    }

    const current = Workspaces.getCtx();
    if (current) {
      setCtx(current);
      return;
    }

    const intervalId = globalThis.setInterval(() => {
      const service = Workspaces.getCtx();
      if (service) {
        setCtx(service);
        globalThis.clearInterval(intervalId);
      }
    }, 500);

    onCleanup(() => globalThis.clearInterval(intervalId));
  });

  const handleCreateWorkspace = () => ctx()?.createNoNameWorkspace();
  return (
    <Show when={shouldShow() ? ctx() : null} keyed>
      {(service) => (
        <>
          <xul:vbox
            id="workspaces-panel-sidebar-section"
            class="panel-sidebar-workspaces"
            flex="0"
          >
            <For each={workspacesDataStore.order}>
              {(workspaceId) => (
                <WorkspacePanelButton workspaceId={workspaceId} ctx={service} />
              )}
            </For>
            <ControlButton
              id="workspaces-panel-create"
              icon="chrome://global/skin/icons/plus.svg"
              label={texts().createNew}
              onActivate={handleCreateWorkspace}
            />
          </xul:vbox>
          <xul:spacer flex="1" />
        </>
      )}
    </Show>
  );
}
