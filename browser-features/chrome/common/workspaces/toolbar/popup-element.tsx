/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, For, Show } from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { PopupToolbarElement } from "./popup-block-element.tsx";
import { configStore } from "../data/config.ts";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type { WorkspaceArchiveSummary } from "../utils/archive-types.ts";

const translationKeys = {
  createNew: "workspaces.popup.create-new",
  manage: "workspaces.popup.manage",
  restore: "workspaces.popup.restore",
  restoreCancel: "workspaces.popup.restore-cancel",
  restoreTitle: "workspaces.popup.restore-title",
  restoreLoading: "workspaces.popup.restore-loading",
  restoreEmpty: "workspaces.popup.restore-empty",
  restoreError: "workspaces.popup.restore-error",
  restoreTooltip: "workspaces.popup.restore-tooltip",
  restoreItem: "workspaces.popup.restore-item",
  restoreDeleteTooltip: "workspaces.popup.restore-delete-tooltip",
  restoreTabCount: "workspaces.popup.restore-tab-count",
  restoreTimeJustNow: "workspaces.popup.restore-time-just-now",
  restoreTimeMinutes: "workspaces.popup.restore-time-minutes",
  restoreTimeHours: "workspaces.popup.restore-time-hours",
  restoreTimeDays: "workspaces.popup.restore-time-days",
};

const translate = (
  key: string,
  options?: Record<string, unknown>,
): string => i18next.t(key, undefined, options) as string;

const getTranslations = () => ({
  createNew: translate(translationKeys.createNew),
  manage: translate(translationKeys.manage),
  restore: translate(translationKeys.restore),
  restoreCancel: translate(translationKeys.restoreCancel),
  restoreTitle: translate(translationKeys.restoreTitle),
  restoreLoading: translate(translationKeys.restoreLoading),
  restoreEmpty: translate(translationKeys.restoreEmpty),
  restoreError: translate(translationKeys.restoreError),
  restoreTooltip: translate(translationKeys.restoreTooltip),
  restoreItem: (name: string) =>
    translate(translationKeys.restoreItem, { name }),
  restoreDeleteTooltip: translate(translationKeys.restoreDeleteTooltip),
  restoreTabCount: (count: number) =>
    translate(translationKeys.restoreTabCount, { count }),
  restoreTimeJustNow: translate(translationKeys.restoreTimeJustNow),
  restoreTimeMinutes: (minutes: number) =>
    translate(translationKeys.restoreTimeMinutes, { minutes }),
  restoreTimeHours: (hours: number) =>
    translate(translationKeys.restoreTimeHours, { hours }),
  restoreTimeDays: (days: number) =>
    translate(translationKeys.restoreTimeDays, { days }),
});

export function PopupElement(props: { ctx: WorkspacesService }) {
  const [texts, setTexts] = createSignal(getTranslations());
  const [isRestoreMode, setIsRestoreMode] = createSignal(false);
  const [archivedWorkspaces, setArchivedWorkspaces] = createSignal<
    WorkspaceArchiveSummary[]
  >([]);
  const [isLoadingRestore, setIsLoadingRestore] = createSignal(false);
  const [restoreError, setRestoreError] = createSignal<string | null>(null);

  const resetRestoreState = () => {
    setIsRestoreMode(false);
    setRestoreError(null);
    setArchivedWorkspaces([]);
    setIsLoadingRestore(false);
  };

  /**
   * Format timestamp as relative time (e.g., "2 hours ago")
   */
  const formatRelativeTime = (timestamp: number): string => {
    try {
      const now = Date.now();
      const diff = now - timestamp;
      const seconds = Math.floor(diff / 1000);
      const minutes = Math.floor(seconds / 60);
      const hours = Math.floor(minutes / 60);
      const days = Math.floor(hours / 24);

      if (seconds < 60) {
        return texts().restoreTimeJustNow;
      }
      if (minutes < 60) {
        return texts().restoreTimeMinutes(minutes);
      }
      if (hours < 24) {
        return texts().restoreTimeHours(hours);
      }
      return texts().restoreTimeDays(days);
    } catch {
      return String(timestamp);
    }
  };

  /**
   * Format timestamp as full date/time (currently unused)
   */
  const _formatCapturedAt = (timestamp: number) => {
    try {
      return new Date(timestamp).toLocaleString();
    } catch {
      return String(timestamp);
    }
  };

  const loadArchivedWorkspaces = async () => {
    setIsLoadingRestore(true);
    setRestoreError(null);
    try {
      const list = await props.ctx.listArchivedWorkspaces();
      setArchivedWorkspaces(list);
    } catch (error) {
      console.error("WorkspacesPopup: failed to load archives", error);
      setRestoreError(texts().restoreError);
    } finally {
      setIsLoadingRestore(false);
    }
  };

  const toggleRestoreMode = async () => {
    if (isRestoreMode()) {
      resetRestoreState();
      return;
    }
    setIsRestoreMode(true);
    await loadArchivedWorkspaces();
  };

  const handleRestore = async (archiveId: string) => {
    setIsLoadingRestore(true);
    setRestoreError(null);
    try {
      const restoredWorkspaceId = await props.ctx.restoreArchivedWorkspace(
        archiveId,
      );
      if (restoredWorkspaceId) {
        resetRestoreState();
      } else {
        await loadArchivedWorkspaces();
      }
    } catch (error) {
      console.error("WorkspacesPopup: failed to restore workspace", error);
      await loadArchivedWorkspaces();
      setRestoreError(texts().restoreError);
    }
  };

  const handleDelete = async (
    archiveId: string,
    event: MouseEvent | Event,
  ) => {
    event.stopPropagation();
    setIsLoadingRestore(true);
    setRestoreError(null);
    try {
      const success = await props.ctx.deleteArchivedWorkspace(archiveId);
      if (success) {
        await loadArchivedWorkspaces();
      } else {
        setRestoreError(texts().restoreError);
      }
    } catch (error) {
      console.error("WorkspacesPopup: failed to delete archive", error);
      setRestoreError(texts().restoreError);
    } finally {
      setIsLoadingRestore(false);
    }
  };

  addI18nObserver(() => {
    setTexts(getTranslations());
  });

  return (
    <xul:panelview
      id="workspacesToolbarButtonPanel"
      type="arrow"
      position="bottomleft topleft"
    >
      <xul:vbox id="workspacesToolbarButtonPanelBox">
        <xul:arrowscrollbox id="workspacesPopupBox" flex="1">
          <xul:vbox
            id="workspacesPopupContent"
            align="center"
            flex="1"
            orient="vertical"
            class="statusbar-padding"
            clicktoscroll
            data-restore-mode={isRestoreMode() ? "true" : "false"}
          >
            <Show
              when={!isRestoreMode()}
              fallback={
                <xul:vbox class="workspaceRestoreContainer">
                  <xul:label class="workspaceRestoreTitle">
                    {texts().restoreTitle}
                  </xul:label>
                  <Show when={restoreError()}>
                    {(message) => (
                      <xul:label class="workspaceRestoreStatus">
                        {message() ?? ""}
                      </xul:label>
                    )}
                  </Show>
                  <Show
                    when={!restoreError() && !isLoadingRestore()}
                    fallback={
                      <xul:label class="workspaceRestoreStatus">
                        {texts().restoreLoading}
                      </xul:label>
                    }
                  >
                    <Show
                      when={archivedWorkspaces().length > 0}
                      fallback={
                        <xul:label class="workspaceRestoreStatus">
                          {texts().restoreEmpty}
                        </xul:label>
                      }
                    >
                      <For each={archivedWorkspaces()}>
                        {(item) => {
                          const iconUrl = () =>
                            props.ctx.iconCtx.getWorkspaceIconUrl(item.icon);
                          return (
                            <xul:hbox
                              class="workspaceRestoreItem"
                              align="center"
                            >
                              <xul:hbox
                                class="workspaceRestoreItemButton"
                                align="center"
                                flex="1"
                                onClick={() => handleRestore(item.archiveId)}
                              >
                                <xul:image
                                  class="workspaceRestoreItemIcon"
                                  src={iconUrl()}
                                />
                                <xul:vbox
                                  class="workspaceRestoreItemContent"
                                  flex="1"
                                >
                                  <xul:label class="workspaceRestoreItemName">
                                    {item.name}
                                  </xul:label>
                                  <xul:hbox class="workspaceRestoreItemMeta">
                                    <xul:label class="workspaceRestoreItemMetaText">
                                      {texts().restoreTabCount(item.tabCount)}
                                    </xul:label>
                                    <xul:label class="workspaceRestoreItemMetaSeparator">
                                      /
                                    </xul:label>
                                    <xul:label class="workspaceRestoreItemMetaText">
                                      {formatRelativeTime(item.capturedAt)}
                                    </xul:label>
                                  </xul:hbox>
                                </xul:vbox>
                              </xul:hbox>
                              <xul:toolbarbutton
                                class="toolbarbutton-1 workspaceRestoreDeleteButton"
                                title={texts().restoreDeleteTooltip}
                                closemenu="none"
                                onClick={(event: MouseEvent) => {
                                  void handleDelete(item.archiveId, event);
                                }}
                              />
                            </xul:hbox>
                          );
                        }}
                      </For>
                    </Show>
                  </Show>
                </xul:vbox>
              }
            >
              <For each={workspacesDataStore.order}>
                {(id) => (
                  <PopupToolbarElement
                    workspaceId={id}
                    isSelected={id === selectedWorkspaceID()}
                    bmsMode={configStore.manageOnBms}
                    ctx={props.ctx}
                  />
                )}
              </For>
            </Show>
          </xul:vbox>
        </xul:arrowscrollbox>
        <xul:toolbarseparator id="workspacesPopupSeparator" />
        <xul:hbox id="workspacesPopupFooter" align="center" pack="center">
          <xul:toolbarbutton
            id="workspacesCreateNewWorkspaceButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            label={texts().createNew}
            context="tab-stacks-toolbar-item-context-menu"
            onCommand={() => props.ctx.createNoNameWorkspace()}
            flex="1"
          />
          <xul:hbox align="center" class="workspaceFooterButtons">
            <xul:toolbarbutton
              id="workspacesRestoreWorkspaceButton"
              class={`toolbarbutton-1 chromeclass-toolbar-additional workspaceRestoreToggle${
                isRestoreMode() ? " workspaceRestoreToggle-active" : ""
              }`}
              title={isRestoreMode()
                ? texts().restoreCancel
                : texts().restoreTooltip}
              closemenu="none"
              onCommand={() => {
                void toggleRestoreMode();
              }}
            />
            <xul:toolbarbutton
              id="workspacesManageworkspacesServicesButton"
              class="toolbarbutton-1 chromeclass-toolbar-additional"
              label={texts().manage}
              context="tab-stacks-toolbar-item-context-menu"
              onCommand={() => props.ctx.manageWorkspaceFromDialog()}
            />
          </xul:hbox>
        </xul:hbox>
      </xul:vbox>
    </xul:panelview>
  );
}
