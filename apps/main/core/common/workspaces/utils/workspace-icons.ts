/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type ModuleStrings = {
  [key: string]: () => Promise<string>;
};

export class WorkspaceIcons {
  private static instance: WorkspaceIcons;
  static getInstance(): WorkspaceIcons {
    if (!WorkspaceIcons.instance) {
      WorkspaceIcons.instance = new WorkspaceIcons();
    }
    return WorkspaceIcons.instance;
  }

  private moduleStrings: ModuleStrings;
  private resolvedIcons: { [key: string]: string } = {};
  private iconsResolved = false;
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

  constructor() {
    this.moduleStrings = import.meta.glob("../icons/*.svg", {
      query: "?url",
      import: "default",
    }) as ModuleStrings;
  }

  public async initializeIcons(): Promise<void> {
    if (this.iconsResolved) return;
    for (const path in this.moduleStrings) {
      const iconUrl = await this.moduleStrings[path]();
      const iconName = path.split("/").pop()?.replace(".svg", "") || "";
      this.resolvedIcons[iconName] = iconUrl;
    }
    this.iconsResolved = true;
  }

  private getUrlBase(): string {
    return import.meta.env.PROD
      ? "chrome://noraneko/content"
      : "http://localhost:5181";
  }

  public getWorkspaceIconUrl(icon: string | null | undefined): string {
    if (!icon || !this.workspaceIcons.has(icon)) {
      return `${this.getUrlBase()}${this.resolvedIcons.fingerprint}`;
    }
    return `${this.getUrlBase()}${this.resolvedIcons[icon]}`;
  }
}
