/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { For } from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { PopupToolbarElement } from "./popup-block-element.tsx";
import { configStore } from "../data/config.ts";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const translationKeys = {
  createNew: "workspaces.popup.create-new",
  manage: "workspaces.popup.manage",
};

const getTranslations = () => ({
  createNew: i18next.t(translationKeys.createNew),
  manage: i18next.t(translationKeys.manage),
});

export function PopupElement(props: { ctx: WorkspacesService }) {
  const [texts, setTexts] = createSignal(getTranslations());
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
            clicktoscroll={true}
            class="statusbar-padding"
          >
            <For each={workspacesDataStore.order}>
              {(id) => {
                return (
                  <PopupToolbarElement
                    workspaceId={id}
                    isSelected={id === selectedWorkspaceID()}
                    bmsMode={configStore.manageOnBms}
                    ctx={props.ctx}
                  />
                );
              }}
            </For>
          </xul:vbox>
        </xul:arrowscrollbox>
        <xul:toolbarseparator id="workspacesPopupSeparator" />
        <xul:hbox id="workspacesPopupFooter" align="center" pack="center">
          <xul:toolbarbutton
            id="workspacesCreateNewWorkspaceButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            data-l10n-id="workspaces-create-new-workspace-button"
            label={texts().createNew}
            context="tab-stacks-toolbar-item-context-menu"
            onCommand={() => props.ctx.createNoNameWorkspace()}
          />
          <xul:toolbarbutton
            id="workspacesManageworkspacesServicesButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            data-l10n-id="workspaces-manage-workspaces-button"
            label={texts().manage}
            context="tab-stacks-toolbar-item-context-menu"
            onCommand={() => props.ctx.manageWorkspaceFromDialog()}
          />
        </xul:hbox>
      </xul:vbox>
    </xul:panelview>
  );
}
