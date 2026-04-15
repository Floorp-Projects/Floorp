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
      query: "?raw",
      import: "default",
      eager: true,
    }) as ModuleStrings;

    for (const path in this.moduleStrings) {
      const iconName = path.split("/").pop()?.replace(".svg", "") ?? "";
      if (iconName) {
        try {
          const svgContent = this.moduleStrings[path];
          const svgBytes = new TextEncoder().encode(svgContent);
          let binString = "";
          svgBytes.forEach((byte) => {
            binString += String.fromCharCode(byte);
          });
          this.resolvedIcons[iconName] = `data:image/svg+xml;base64,${btoa(
            binString,
          )}`;
        } catch (e) {
          console.error(`Failed to process and encode icon: ${iconName}`, e);
        }
      }
    }
  }

  public getWorkspaceIconUrl(icon: string | null | undefined): string {
    if (!icon || !this.workspaceIcons.has(icon)) {
      return this.resolvedIcons.fingerprint;
    }
    return this.resolvedIcons[icon];
  }
}
