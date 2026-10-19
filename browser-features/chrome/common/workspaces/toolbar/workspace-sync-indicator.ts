// SPDX-License-Identifier: MPL-2.0

import {
  type Accessor,
  createEffect,
  createMemo,
  createSignal,
  onCleanup,
} from "solid-js";
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
      return {
        state: undefined,
        status: i18next.t("workspaces.sync.status-local"),
        error: null,
        tooltip: id
          ? i18next.t("workspaces.sync.indicator-local", { name: name() })
          : name(),
      };
    }
    const error = ctx.syncController.errorText(id) || null;
    return {
      state: error ? "attention" : "selected",
      status: i18next.t(
        error
          ? "workspaces.sync.status-error"
          : "workspaces.sync.status-selected",
      ),
      error,
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

export function bindWorkspaceSyncFocusTooltip(
  node: Element,
  indicator: Accessor<WorkspaceSyncIndicator>,
): void {
  const targetDocument = node.ownerDocument;
  const tooltip = targetDocument.createXULElement("tooltip") as XULPopupElement;
  tooltip.id = `workspace-sync-tooltip-${
    Services.uuid.generateUUID().toString().slice(1, -1)
  }`;
  tooltip.classList.add("workspace-sync-focus-tooltip");
  tooltip.setAttribute("role", "tooltip");
  tooltip.setAttribute("noautofocus", "true");
  const reason = targetDocument.createXULElement("description");
  reason.classList.add("workspace-sync-focus-reason");
  tooltip.appendChild(reason);
  (targetDocument.getElementById("mainPopupSet") ??
    targetDocument.documentElement).appendChild(tooltip);

  const attribute = "data-workspace-sync-tooltip";
  const previousId = node.getAttribute(attribute);
  node.setAttribute(attribute, tooltip.id);
  let dismissed = false;

  const hide = () => {
    if (tooltip.state !== "closed") tooltip.hidePopup();
  };
  const show = () => {
    const current = indicator();
    if (
      dismissed || !node.isConnected || targetDocument.activeElement !== node ||
      current.state !== "attention" || !current.error
    ) {
      hide();
      return;
    }
    reason.textContent = current.error;
    if (tooltip.state === "closed") {
      tooltip.openPopup(node, "after_start", 0, 0, false, false);
    }
  };
  const handleFocus = () => {
    dismissed = false;
    show();
  };
  const handleBlur = () => {
    dismissed = false;
    hide();
  };
  const handleKeyDown = (event: KeyboardEvent) => {
    if (event.key === "Escape") {
      dismissed = true;
      hide();
    }
  };
  const handlePopupHidden = (event: Event) => {
    const popup = event.target;
    if (popup instanceof Element && popup !== tooltip && popup.contains(node)) {
      hide();
    }
  };

  node.addEventListener("focus", handleFocus);
  node.addEventListener("blur", handleBlur);
  targetDocument.addEventListener("keydown", handleKeyDown, true);
  targetDocument.addEventListener("popuphidden", handlePopupHidden, true);
  targetDocument.defaultView?.addEventListener("blur", hide);
  createEffect(() => {
    const current = indicator();
    reason.textContent = current.error ?? "";
    show();
  });
  onCleanup(() => {
    node.removeEventListener("focus", handleFocus);
    node.removeEventListener("blur", handleBlur);
    targetDocument.removeEventListener("keydown", handleKeyDown, true);
    targetDocument.removeEventListener("popuphidden", handlePopupHidden, true);
    targetDocument.defaultView?.removeEventListener("blur", hide);
    hide();
    tooltip.remove();
    if (node.getAttribute(attribute) === tooltip.id) {
      if (previousId === null) node.removeAttribute(attribute);
      else node.setAttribute(attribute, previousId);
    }
  });
}
