/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type ModuleStrings = {
  [key: string]: string;
};

export class WorkspaceIcons {
  private moduleStrings: ModuleStrings;
  private resolvedIcons: { [key: string]: string } = {};
  public workspaceIcons = new Set([
    "article",
    "book",
    "briefcase",
    "cart",
    "chat",
    "chill",
    "circle",
    "compass",
    "code",
    "dollar",
    "fence",
    "fingerprint",
    "food",
    "fruit",
    "game",
    "gear",
    "gift",
    "key",
    "lightning",
    "network",
    "notes",
    "paint",
    "photo",
    "pin",
    "pet",
    "question",
    "smartphone",
    "star",
    "tree",
    "vacation",
    "love",
    "moon",
    "music",
    "user",
  ]);

  get workspaceIconsArray(): string[] {
    return Array.from(this.workspaceIcons);
  }

  constructor() {
    this.moduleStrings = import.meta.glob("../icons/*.svg", {
      query: "?url",
      import: "default",
      eager: true,
    }) as ModuleStrings;

    const isDevelopment = import.meta.env?.MODE === "dev" || false;
    for (const path in this.moduleStrings) {
      const iconUrl = isDevelopment
        ? `http://localhost:5181/core/common/workspaces/icons/${
          path.split("/").pop()
        }`
        : this.moduleStrings[path];
      const iconName = path.split("/").pop()?.replace(".svg", "") || "";
      this.resolvedIcons[iconName] = iconUrl;
    }
  }

  public getWorkspaceIconUrl(icon: string | null | undefined): string {
    if (!icon || !this.workspaceIcons.has(icon)) {
      return this.resolvedIcons.fingerprint;
    }
    return this.resolvedIcons[icon];
  }
}
