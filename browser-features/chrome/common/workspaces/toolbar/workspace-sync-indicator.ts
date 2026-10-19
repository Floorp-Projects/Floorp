// SPDX-License-Identifier: MPL-2.0

import { type Accessor, createMemo, createSignal } from "solid-js";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import type { WorkspaceSyncIndicator } from "./workspace-sync-indicator-types.ts";

export function createWorkspaceSyncIndicator(
  ctx: WorkspacesService,
  workspaceId: Accessor<TWorkspaceID | undefined>,
  name: Accessor<string>,
): Accessor<WorkspaceSyncIndicator> {
  const [locale, setLocale] = createSignal(i18next.language);
  addI18nObserver(setLocale);

  return createMemo<WorkspaceSyncIndicator>(() => {
    locale();
    const id = workspaceId();
    if (!id || !ctx.syncController?.isSelected(id)) {
      return { state: undefined, tooltip: name() };
    }
    const error = ctx.syncController.errorText(id);
    return {
      state: error ? "attention" : "selected",
      tooltip: error
        ? i18next.t("workspaces.sync.indicator-attention", {
          name: name(),
          error,
        })
        : i18next.t("workspaces.sync.indicator-selected", { name: name() }),
    };
  });
}

export function applyWorkspaceSyncIndicator(
  node: Element,
  indicator: WorkspaceSyncIndicator,
): void {
  if (indicator.state) {
    node.setAttribute("data-workspace-sync", indicator.state);
  } else {
    node.removeAttribute("data-workspace-sync");
  }
  node.setAttribute("tooltiptext", indicator.tooltip);
  node.setAttribute("aria-label", indicator.tooltip);
}
