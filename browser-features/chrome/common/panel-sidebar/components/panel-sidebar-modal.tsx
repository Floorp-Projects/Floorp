/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { type Accessor, createSignal } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import type { Panel } from "../utils/type.ts";
import { getFirefoxSidebarPanels } from "../extension-panels.ts";
import { STATIC_PANEL_DATA } from "../data/static-panels.ts";
import { setPanelSidebarData } from "../data/data.ts";
import ModalParent from "../../../common/modal-parent/index.ts";
import type {
  TForm,
  TFormItem,
  TFormResult,
} from "@core/common/modal-parent/utils/type.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

type Container = {
  name: string;
  userContextId: number;
  l10nId?: string;
};

type I18nTextValues = {
  type: string;
  typeOptionWeb: string;
  typeOptionStatic: string;
  typeOptionExtension: string;
  width: string;
  url: string;
  container: string;
  noContainer: string;
  userAgent: string;
  extension: string;
  sideBarTool: string;
  title: string;
  add: string;
  cancel: string;
};

const translationKeys = {
  type: "panelSidebar.modal.type",
  typeOptionWeb: "panelSidebar.modal.typeOptions.web",
  typeOptionStatic: "panelSidebar.modal.typeOptions.sidebar-tool",
  typeOptionExtension: "panelSidebar.modal.typeOptions.extension",
  width: "panelSidebar.modal.width",
  url: "panelSidebar.modal.url",
  container: "panelSidebar.modal.container",
  noContainer: "panelSidebar.modal.noContainer",
  userAgent: "panelSidebar.modal.userAgent",
  extension: "panelSidebar.modal.extension",
  sideBarTool: "panelSidebar.modal.sideBarTool",
  title: "panelSidebar.modal.title",
  add: "panelSidebar.modal.add",
  cancel: "panelSidebar.modal.cancel",
};

const getTranslatedTexts = (): I18nTextValues => {
  return {
    type: i18next.t(translationKeys.type),
    typeOptionWeb: i18next.t(translationKeys.typeOptionWeb),
    typeOptionStatic: i18next.t(translationKeys.typeOptionStatic),
    typeOptionExtension: i18next.t(translationKeys.typeOptionExtension),
    width: i18next.t(translationKeys.width),
    url: i18next.t(translationKeys.url),
    container: i18next.t(translationKeys.container),
    noContainer: i18next.t(translationKeys.noContainer),
    userAgent: i18next.t(translationKeys.userAgent),
    extension: i18next.t(translationKeys.extension),
    sideBarTool: i18next.t(translationKeys.sideBarTool),
    title: i18next.t(translationKeys.title),
    add: i18next.t(translationKeys.add),
    cancel: i18next.t(translationKeys.cancel),
  };
};

function completeUrl(url: string): string {
  if (!url) return url;

  if (url.startsWith("http://") || url.startsWith("https://")) {
    return url;
  }

  return `https://${url}`;
}

export class PanelSidebarAddModal {
  private static instance: PanelSidebarAddModal;
  private modalParent: ModalParent;

  private texts: Accessor<I18nTextValues> = () => getTranslatedTexts();
  private setTexts: (value: I18nTextValues) => void = () => {};

  public static getInstance() {
    if (!PanelSidebarAddModal.instance) {
      PanelSidebarAddModal.instance = new PanelSidebarAddModal();
    }
    return PanelSidebarAddModal.instance;
  }

  constructor() {
    this.modalParent = ModalParent.getInstance();
    this.modalParent.init();

    createRootHMR(
      () => {
        const [texts, setTexts] = createSignal<I18nTextValues>(
          getTranslatedTexts(),
        );
        this.texts = texts;
        this.setTexts = setTexts;

        addI18nObserver(() => {
          setTexts(getTranslatedTexts());
        });
      },
      import.meta.hot,
    );
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

  private createFormConfig(type: Panel["type"] = "web"): TForm {
    const extensions = getFirefoxSidebarPanels();
    const texts = this.texts();
    const staticPanelOptions = Object.entries(STATIC_PANEL_DATA).map(
      ([key, panel]) => ({
        value: key,
        label: panel.l10n,
        icon: "",
      }),
    );

    const containerOptions = [
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
    ];

    const extensionOptions = extensions.map((extension) => ({
      value: extension.extensionId,
      label: extension.title,
      icon: extension.iconUrl,
    }));

    const commonForms: TFormItem[] = [
      {
        id: "type",
        type: "dropdown",
        label: texts.type,
        value: type,
        required: true,
        options: [
          { value: "web", label: texts.typeOptionWeb, icon: "" },
          { value: "static", label: texts.typeOptionStatic, icon: "" },
          { value: "extension", label: texts.typeOptionExtension, icon: "" },
        ],
      },
      {
        id: "width",
        type: "number",
        label: texts.width,
        value: 450,
        required: true,
      },
    ];

    const webForms: TFormItem[] = [
      {
        id: "url",
        type: "url",
        label: texts.url,
        value: window.gBrowser.currentURI.spec,
        required: true,
        placeholder: "https://floorp.app",
        when: { id: "type", value: "web" },
        onInput: (value: string) => {
          if (value) {
            return completeUrl(value);
          }
          return value;
        },
      },
      {
        id: "userContextId",
        type: "dropdown",
        label: texts.container,
        value: "0",
        required: true,
        options: containerOptions,
        when: { id: "type", value: "web" },
      },
      {
        id: "userAgent",
        type: "checkbox",
        label: texts.userAgent,
        value: "false",
        when: { id: "type", value: "web" },
      },
    ];

    // 拡張機能パネル用のフォーム項目
    const extensionForms: TFormItem[] = [
      {
        id: "extension",
        type: "dropdown",
        label: texts.extension,
        value: extensions.length > 0 ? extensions[0].extensionId : "",
        required: true,
        options: extensionOptions,
        when: { id: "type", value: "extension" },
      },
    ];

    // 静的パネル用のフォーム項目
    const staticForms: TFormItem[] = [
      {
        id: "sideBarTool",
        type: "dropdown",
        label: texts.sideBarTool,
        value: Object.keys(STATIC_PANEL_DATA)[0] || "",
        required: true,
        options: staticPanelOptions,
        when: { id: "type", value: "static" },
      },
    ];

    // タイプに関係なく全てのフォーム項目を結合
    const allForms = [
      ...commonForms,
      ...webForms,
      ...extensionForms,
      ...staticForms,
    ];

    return {
      forms: allForms,
      title: texts.title,
      submitLabel: texts.add,
      cancelLabel: texts.cancel,
    };
  }

  public async showAddPanelModal(): Promise<TFormResult | null> {
    const formConfig = this.createFormConfig();
    return await new Promise((resolve) => {
      this.modalParent.showNoraModal(
        formConfig,
        {
          width: 600,
          height: 620,
        },
        (result: TFormResult | null) => {
          if (result) {
            const type = result.type as Panel["type"];
            let panel: Panel = {
              type,
              id: crypto.randomUUID(),
              width: Number(result.width) || 450,
            };

            if (type === "web") {
              panel = {
                ...panel,
                url: completeUrl(result.url as string),
                userContextId: Number(result.userContextId),
                userAgent: result.userAgent === "true",
              };
            }

            if (type === "extension") {
              panel = {
                ...panel,
                extensionId: result.extension as string,
              };
            }

            if (type === "static") {
              const sideBarToolKey = result
                .sideBarTool as keyof typeof STATIC_PANEL_DATA;
              panel = {
                ...panel,
                url: sideBarToolKey,
              };
            }

            setPanelSidebarData((prev) => [...prev, panel]);
          }
          resolve(result);
        },
      );
    });
  }
}

export function showPanelSidebarAddModal(): Promise<TFormResult | null> {
  return PanelSidebarAddModal.getInstance().showAddPanelModal();
}
