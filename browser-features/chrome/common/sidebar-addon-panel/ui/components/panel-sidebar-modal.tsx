/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  panelSidebarData,
  setPanelSidebarData,
  setSelectedPanelId,
} from "../../core/data.ts";
import type { Panel } from "../../core/utils/type.ts";

/**
 * Minimal synchronous modal replacement.
 * Uses simple prompt dialogs to collect a URL and type, then appends a panel.
 * Keeps API surface intact: showPanelSidebarAddModal() returns a Promise<null>.
 */

export class PanelSidebarAddModal {
  private static instance: PanelSidebarAddModal;

  public static getInstance() {
    if (!PanelSidebarAddModal.instance) {
      PanelSidebarAddModal.instance = new PanelSidebarAddModal();
    }
    return PanelSidebarAddModal.instance;
  }

  constructor() {
    // Intentionally minimal. Real dialog can be implemented later if needed.
  }

  public async showAddPanelModal(): Promise<null> {
    const url = window.prompt(
      "Enter panel URL (e.g. https://example.com or floorp//notes):",
      "",
    );
    if (!url) return null;

    let type = (
      window.prompt("Panel type — one of: web, static, extension", "web") ||
      "web"
    ).trim();
    if (!["web", "static", "extension"].includes(type)) {
      type = "web";
    }

    const id = `panel-${Date.now()}`;
    const panel: Panel = {
      id,
      type: type as any,
      width: 0,
      url,
      userContextId: 0,
      zoomLevel: null,
      userAgent: false,
      extensionId: type === "extension" ? "" : undefined,
    };

    setPanelSidebarData((prev: Panel[]) => [...prev, panel]);
    // Select the newly added panel
    setSelectedPanelId(id);

    return null;
  }
}

export function showPanelSidebarAddModal(): Promise<null> {
  return PanelSidebarAddModal.getInstance().showAddPanelModal();
}
