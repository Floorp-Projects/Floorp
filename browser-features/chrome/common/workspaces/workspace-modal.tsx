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
import {
  getWorkspaceSessionIdentities,
  getWorkspaceSessionName,
  WORKSPACE_NEW_SESSION_VALUE,
} from "./utils/workspace-sessions.ts";
import type {
  WorkspaceModalMode,
  WorkspaceModalTexts,
  WorkspaceSessionFormLabels,
  WorkspaceSessionOption,
} from "./utils/workspace-modal-types.ts";

export { WORKSPACE_ICON_NO_CHANGE_SENTINEL } from "./utils/workspace-icons.ts";

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

export const createWorkspaceSessionFormItem = (
  userContextId: number,
  sessions: readonly WorkspaceSessionOption[],
  labels: WorkspaceSessionFormLabels,
  mode: WorkspaceModalMode,
): TFormItem => {
  const hasCurrentSession = userContextId === 0 ||
    sessions.some((session) => session.userContextId === userContextId);
  const options = [
    { value: "0", label: labels.standard },
    ...sessions.map((session) => ({
      value: session.userContextId.toString(),
      label: session.name,
    })),
  ];
  const currentValue = userContextId.toString();
  const currentOption = options.find((option) => option.value === currentValue);
  const otherOptions = mode === "create" && hasCurrentSession
    ? options.filter((option) => option.value !== currentValue)
    : options;

  return {
    id: "userContextId",
    type: "select",
    label: labels.label,
    value: hasCurrentSession ? currentValue : "",
    required: true,
    options: [
      ...(!hasCurrentSession
        ? [{ value: "", label: labels.chooseSession }]
        : mode === "create" && currentOption
        ? [{ ...currentOption, label: labels.shareCurrent }]
        : []),
      { value: WORKSPACE_NEW_SESSION_VALUE, label: labels.newSession },
      ...otherOptions,
    ],
  };
};

const translationKeys = {
  name: "workspaces.modal.name",
  namePlaceholder: "workspaces.modal.name-placeholder",
  container: "workspaces.modal.container",
  noContainer: "workspaces.modal.no-container",
  icon: "workspaces.modal.icon",
  editTitle: "workspaces.modal.edit-title",
  createTitle: "workspaces.modal.create-title",
  createDescription: "workspaces.modal.create-description",
  editDescription: "workspaces.modal.edit-description",
  create: "workspaces.modal.create",
  save: "workspaces.modal.save",
  cancel: "workspaces.modal.cancel",
  newSession: "workspaces.modal.new-session",
  chooseSession: "workspaces.modal.choose-session",
};

const getTranslatedTexts = (): WorkspaceModalTexts => {
  return {
    name: i18next.t(translationKeys.name),
    namePlaceholder: i18next.t(translationKeys.namePlaceholder),
    container: i18next.t(translationKeys.container),
    noContainer: i18next.t(translationKeys.noContainer),
    icon: i18next.t(translationKeys.icon),
    editTitle: i18next.t(translationKeys.editTitle),
    createTitle: i18next.t(translationKeys.createTitle),
    createDescription: i18next.t(translationKeys.createDescription),
    editDescription: i18next.t(translationKeys.editDescription),
    create: i18next.t(translationKeys.create),
    save: i18next.t(translationKeys.save),
    cancel: i18next.t(translationKeys.cancel),
    newSession: i18next.t(translationKeys.newSession),
    chooseSession: i18next.t(translationKeys.chooseSession),
  };
};

export class WorkspaceManageModal {
  ctx: WorkspacesService;
  iconCtx: WorkspaceIcons;
  private modalParent: ModalParent;
  private iconTranslationsHandler: IconTranslationsHandler;

  private texts: Accessor<WorkspaceModalTexts> = () => getTranslatedTexts();
  private setTexts: (value: WorkspaceModalTexts) => void = () => {};

  constructor(ctx: WorkspacesService, iconCtx: WorkspaceIcons) {
    this.ctx = ctx;
    this.iconCtx = iconCtx;
    this.modalParent = ModalParent.getInstance();
    this.modalParent.init();
    this.iconTranslationsHandler = IconTranslationsHandler.getInstance();

    createRootHMR(() => {
      const [texts, setTexts] = createSignal<WorkspaceModalTexts>(
        getTranslatedTexts(),
      );
      this.texts = texts;
      this.setTexts = setTexts;

      addI18nObserver(() => {
        setTexts(getTranslatedTexts());
      });
    }, import.meta.hot);
  }

  private createFormConfig(
    workspace: TWorkspace,
    mode: WorkspaceModalMode = "edit",
  ): TForm {
    const texts = this.texts();
    const sessions = getWorkspaceSessionIdentities().map((identity) => ({
      userContextId: identity.userContextId,
      name: getWorkspaceSessionName(identity.userContextId) ?? identity.name,
    }));
    const currentSessionName = workspace.userContextId === 0
      ? texts.noContainer
      : sessions.find((session) =>
        session.userContextId === workspace.userContextId
      )?.name;

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
        createWorkspaceSessionFormItem(workspace.userContextId, sessions, {
          label: texts.container,
          standard: texts.noContainer,
          shareCurrent: i18next.t("workspaces.modal.share-session", {
            name: currentSessionName ?? "",
          }),
          newSession: texts.newSession,
          chooseSession: texts.chooseSession,
        }, mode),
        createWorkspaceIconPickerFormItem(
          workspace,
          this.iconCtx,
          (alias) => this.iconTranslationsHandler.getTranslatedIconName(alias),
          texts.icon,
        ),
      ],
      title: mode === "create" ? texts.createTitle : texts.editTitle,
      description: mode === "create"
        ? texts.createDescription
        : texts.editDescription,
      submitLabel: mode === "create" ? texts.create : texts.save,
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
    return await this.showForm(this.createFormConfig(workspace));
  }

  public async showCreateWorkspacesModal(
    seed: TWorkspace,
  ): Promise<TFormResult | null> {
    return await this.showForm(this.createFormConfig(seed, "create"));
  }

  private async showForm(form: TForm): Promise<TFormResult | null> {
    return await this.modalParent.showNoraModal(form, {
      width: 540,
      height: 700,
    }, () => {});
  }
}
