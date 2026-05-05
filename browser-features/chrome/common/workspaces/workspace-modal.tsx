/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { WorkspaceIcons } from "./utils/workspace-icons.ts";
import type { TWorkspace, TWorkspaceID } from "./utils/type.ts";
import type { WorkspacesService } from "./workspacesService.ts";
import ModalParent from "../modal-parent/index.ts";
import type {
  TForm,
  TFormResult,
} from "#features-chrome/common/modal-parent/utils/type.ts";
import i18next from "i18next";
import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { createRootHMR } from "#features-chrome/utils/base";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { IconTranslationsHandler } from "./utils/icon-translations-handler.ts";

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

type Container = {
  name: string;
  userContextId: number;
  l10nId?: string;
};

type I18nTextValues = {
  name: string;
  namePlaceholder: string;
  container: string;
  noContainer: string;
  icon: string;
  editTitle: string;
  save: string;
  cancel: string;
};

const translationKeys = {
  name: "workspaces.modal.name",
  namePlaceholder: "workspaces.modal.name-placeholder",
  container: "workspaces.modal.container",
  noContainer: "workspaces.modal.no-container",
  icon: "workspaces.modal.icon",
  editTitle: "workspaces.modal.edit-title",
  save: "workspaces.modal.save",
  cancel: "workspaces.modal.cancel",
};

const getTranslatedTexts = (): I18nTextValues => {
  return {
    name: i18next.t(translationKeys.name),
    namePlaceholder: i18next.t(translationKeys.namePlaceholder),
    container: i18next.t(translationKeys.container),
    noContainer: i18next.t(translationKeys.noContainer),
    icon: i18next.t(translationKeys.icon),
    editTitle: i18next.t(translationKeys.editTitle),
    save: i18next.t(translationKeys.save),
    cancel: i18next.t(translationKeys.cancel),
  };
};

export class WorkspaceManageModal {
  ctx: WorkspacesService;
  iconCtx: WorkspaceIcons;
  private modalParent: ModalParent;
  private iconTranslationsHandler: IconTranslationsHandler;

  private textsSignal: Signal<I18nTextValues>;

  constructor(ctx: WorkspacesService, iconCtx: WorkspaceIcons) {
    this.ctx = ctx;
    this.iconCtx = iconCtx;
    this.modalParent = ModalParent.getInstance();
    this.modalParent.init();
    this.iconTranslationsHandler = IconTranslationsHandler.getInstance();

    this.textsSignal = createRootHMR(() => {
      const sig = signal<I18nTextValues>(getTranslatedTexts());
      addI18nObserver(() => {
        sig.value = getTranslatedTexts();
      });
      return sig;
    }, import.meta.hot);
  }

  private get containers(): Container[] {
    return ContextualIdentityService.getPublicIdentities();
  }

  private getContainerName(container: Container) {
    if (container.l10nId) {
      return ContextualIdentityService.getUserContextLabel(
        container.userContextId,
      );
    }
    return container.name;
  }

  private createFormConfig(workspace: TWorkspace): TForm {
    const texts = this.textsSignal.value;

    return {
      forms: [
        {
          id: "name",
          type: "text",
          label: texts.name,
          value: workspace.name,
          required: true,
          placeholder: texts.namePlaceholder,
        },
        {
          id: "userContextId",
          type: "dropdown",
          label: texts.container,
          value: workspace.userContextId?.toString() || "0",
          required: true,
          options: [
            {
              value: "0",
              label: texts.noContainer,
              icon: "",
            },
            ...this.containers.map((container) => ({
              value: container.userContextId.toString(),
              label: this.getContainerName(container),
              icon: "",
            })),
          ],
        },
        {
          id: "icon",
          type: "dropdown",
          label: texts.icon,
          value: workspace.icon || "fingerprint",
          required: true,
          options: this.iconCtx.workspaceIconsArray.map((iconName) => ({
            value: iconName,
            label: this.iconTranslationsHandler.getTranslatedIconName(iconName),
            icon: this.iconCtx.getWorkspaceIconUrl(iconName),
          })),
        },
      ],
      title: texts.editTitle,
      submitLabel: texts.save,
      cancelLabel: texts.cancel,
    };
  }

  public async showWorkspacesModal(
    workspaceID: TWorkspaceID | null,
  ): Promise<TFormResult | null> {
    const workspace = this.ctx.getRawWorkspace(
      workspaceID ?? this.ctx.getSelectedWorkspaceID(),
    );
    if (!workspace) return null;
    const formConfig = this.createFormConfig(workspace);
    return await new Promise((resolve) => {
      this.modalParent.showNoraModal(formConfig, {
        width: 540,
        height: 500,
      }, (result: TFormResult | null) => resolve(result));
    });
  }
}
