/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

/* zod schemas */
export const zWorkspace = z.object({
  id: z.string(),
  name: z.string(),
  icon: z.string().nullish(),
  emoji: z.string().nullish(),
  color: z.string(),
});

export const zWorkspaces = z.object({
  workspaces: z.array(zWorkspace),
  currentWorkspaceId: z.string(),
});

export const zWorkspacesStoreData = z.array(zWorkspace);
export const zWorkspaceBackupTab = z.object({
  title: z.string(),
  url: z.string(),
  favicon: z.string(),
  pinned: z.boolean(),
  selected: z.boolean(),
});

export const zWorkspaceBackup = z.object({
  workspace: z.array(
    zWorkspace.merge(
      z.object({
        tabs: z.array(zWorkspaceBackupTab),
      }),
    ),
  ),
});

export const zWorkspacesBackup = z.object({
  workspaces: z.array(zWorkspaceBackup),
  currentWorkspaceId: z.string(),
  timestamp: z.number(),
});

export const zWorkspacesConfigs = z.object({
  manageOnBms: z.boolean(),
  showWorkspaceNameOnToolbar: z.boolean(),
  closePopupAfterClick: z.boolean(),
});

/* Export as types */
export type workspace = z.infer<typeof zWorkspace>;
export type workspaces = z.infer<typeof zWorkspaces>;
export type workspacesStoreData = z.infer<typeof zWorkspacesStoreData>;
export type workspaceBackupTab = z.infer<typeof zWorkspaceBackupTab>;
export type workspaceBackup = z.infer<typeof zWorkspaceBackup>;
export type workspacesBackup = z.infer<typeof zWorkspacesBackup>;
export type zWorkspacesConfigsType = z.infer<typeof zWorkspacesConfigs>;
