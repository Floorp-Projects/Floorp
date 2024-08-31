/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const workspaceIcons = new Set([
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

export function getWorkspaceIconUrl(icon: string) {
  if (!workspaceIcons.has(icon) || !icon) {
    return "chrome://floorp/skin/workspace-icons/fingerprint.svg";
  }
  return `chrome://floorp/skin/workspace-icons/${icon}.svg`;
}
