// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import {
  getWorkspaceSessionIdentityService,
} from "../workspaces/utils/workspace-sessions.ts";
import { getWorkspaceSessionRuntime } from "../workspaces/utils/workspace-session-runtime.ts";
import type { SessionIndicatorState, SessionIndicatorTab } from "./types.ts";

export function sessionName(id: number): string {
  if (id === 0) {
    return i18next.t("workspaces.session.standard", {
      defaultValue: "Standard",
    });
  }
  return (getWorkspaceSessionRuntime().isValidContext(id)
    ? getWorkspaceSessionIdentityService().getUserContextLabel(id)
    : null) ||
    i18next.t("workspaces.session.unavailable", {
      defaultValue: "Unavailable session ({{id}})",
      id,
    });
}

export function workspaceSessionLabel(name: string, id: number): string {
  return `${name} — ${sessionName(id)}`;
}

/** Read the browser's actual origin context; the workspace only defines defaults. */
export function tabSessionState(
  tab: SessionIndicatorTab,
  defaultId: number,
): SessionIndicatorState {
  const pendingId = tab.getAttribute("floorpPendingSessionId");
  const pending = pendingId !== null ||
    tab.hasAttribute("floorpPendingSessionState");
  const raw = pendingId ?? tab.linkedBrowser.getAttribute("usercontextid") ??
    "0";
  const browserId = tab.linkedBrowser.browsingContext?.originAttributes
    ?.userContextId;
  const actualId =
    !pending && Number.isSafeInteger(browserId) && browserId! >= 0
      ? browserId!
      : /^(0|[1-9]\d*)$/.test(raw)
      ? Number(raw)
      : 0;
  const missing = pending ||
    !getWorkspaceSessionRuntime().isValidContext(actualId);
  const name = sessionName(actualId);
  const exceptional = actualId !== defaultId;
  const label = pending
    ? i18next.t("workspaces.session.pending-label", {
      defaultValue:
        "{{name}}: page loading is paused. Choose a session to reopen it.",
      name,
    })
    : missing
    ? i18next.t("workspaces.session.loaded-unavailable", {
      defaultValue:
        "{{name}}: this existing tab keeps its original environment.",
      name,
    })
    : i18next.t("workspaces.session.actual-label", {
      defaultValue: "Session: {{name}}{{exception}}",
      name,
      exception: exceptional
        ? i18next.t("workspaces.session.exception", {
          defaultValue: " (workspace exception)",
        })
        : "",
    });
  return {
    actualId,
    defaultId,
    pending,
    unavailable: missing,
    exceptional,
    name,
    label,
  };
}
