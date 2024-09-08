/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type ModuleStrings = {
  [key: string]: () => Promise<string>;
};

const moduleStrings = import.meta.glob("../icons/*.svg", {
  query: "?url",
  import: "default",
}) as ModuleStrings;

const resolvedIcons: { [key: string]: string } = {};
let iconsResolved = false;

async function initializeIcons(): Promise<void> {
  if (iconsResolved) return;
  for (const path in moduleStrings) {
    const iconUrl = await moduleStrings[path]();
    const iconName = path.split("/").pop()?.replace(".svg", "") || "";
    resolvedIcons[iconName] = iconUrl;
  }
  iconsResolved = true;
}

export const workspaceIcons: Set<string> = new Set([
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

function getUrlBase(): string {
  return import.meta.env.PROD
    ? "chrome://noraneko/content"
    : "http://localhost:5181";
}

export function getWorkspaceIconUrl(icon: string | null | undefined): string {
  if (!icon || !workspaceIcons.has(icon)) {
    return `${getUrlBase()}${resolvedIcons.fingerprint}`;
  }
  return `${getUrlBase()}${resolvedIcons[icon]}`;
}

initializeIcons();
