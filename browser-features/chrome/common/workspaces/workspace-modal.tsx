/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  applyWorkspaceIconSelection,
  WORKSPACE_ICON_NO_CHANGE_SENTINEL,
  type WorkspaceIcons,
} from "./utils/workspace-icons.ts";
import type { TWorkspace, TWorkspaceID } from "./utils/type.ts";
import type { WorkspacesService } from "./workspacesService.ts";
import ModalParent from "../modal-parent/index.ts";
import type {
  TForm,
  TFormItem,
  TFormResult,
} from "#features-chrome/common/modal-parent/utils/type.ts";
import i18next from "i18next";
import { type Accessor, createSignal } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { IconTranslationsHandler } from "./utils/icon-translations-handler.ts";

export {
  WORKSPACE_ICON_NO_CHANGE_SENTINEL,
} from "./utils/workspace-icons.ts";

export const createWorkspaceIconPickerFormItem = (
  workspace: TWorkspace,
  iconCtx: WorkspaceIcons,
  getTranslatedIconName: (alias: string) => string,
  label: string,
): TFormItem => ({
  id: "icon",
  type: "workspace-icon-picker",
  label,
  value: WORKSPACE_ICON_NO_CHANGE_SENTINEL,
  displayValue: iconCtx.getWorkspaceIconCanonicalId(workspace.icon),
  options: iconCtx.registry.map((entry) => ({
    value: entry.canonicalId,
    label: getTranslatedIconName(entry.alias),
    icon: iconCtx.getWorkspaceIconUrl(entry.canonicalId),
    keywords: [...entry.keywords],
  })),
});

export const applyWorkspaceModalResult = (
  workspace: TWorkspace,
  result: TFormResult | null,
): TWorkspace | null => {
  if (result === null) {
    return null;
  }
  return applyWorkspaceIconSelection(
    {
      ...workspace,
      name: result.name as string,
      userContextId: Number(result.userContextId),
    },
    result.icon,
  );
};

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
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

  private texts: Accessor<I18nTextValues> = () => getTranslatedTexts();
  private setTexts: (value: I18nTextValues) => void = () => {};

  constructor(ctx: WorkspacesService, iconCtx: WorkspaceIcons) {
    this.ctx = ctx;
    this.iconCtx = iconCtx;
    this.modalParent = ModalParent.getInstance();
    this.modalParent.init();
    this.iconTranslationsHandler = IconTranslationsHandler.getInstance();

    createRootHMR(() => {
      const [texts, setTexts] = createSignal<I18nTextValues>(
        getTranslatedTexts(),
      );
      this.texts = texts;
      this.setTexts = setTexts;

      addI18nObserver(() => {
        setTexts(getTranslatedTexts());
      });
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
    const texts = this.texts();

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
        createWorkspaceIconPickerFormItem(
          workspace,
          this.iconCtx,
          (alias) => this.iconTranslationsHandler.getTranslatedIconName(alias),
          texts.icon,
        ),
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
        height: 700,
      }, (result: TFormResult | null) => resolve(result));
    });
  }
}
