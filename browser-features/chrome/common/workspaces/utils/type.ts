/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as t from "io-ts";

/* io-ts codecs */
export const zWorkspace = t.type({
  name: t.string,
  icon: t.union([t.string, t.null, t.undefined]),
  userContextId: t.number,
  isSelected: t.union([t.boolean, t.null, t.undefined]),
  isDefault: t.union([t.boolean, t.null, t.undefined]),
});

// Brand type for WorkspaceID
export type WorkspaceIDBrand = { readonly WorkspaceID: unique symbol };
export const zWorkspaceID = t.brand(
  t.string,
  (s): s is t.Branded<string, WorkspaceIDBrand> =>
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(s),
  "WorkspaceID",
);

export const zWorkspacesServicesStoreData = t.type({
  defaultID: zWorkspaceID,
  data: t.UnknownRecord, // Map is not directly supported, using record
  order: t.array(zWorkspaceID),
});

export const zWorkspaceBackupTab = t.type({
  title: t.string,
  url: t.string,
  favicon: t.string,
  pinned: t.boolean,
  selected: t.boolean,
});

export const zWorkspaceBackup = t.type({
  workspace: t.array(
    t.intersection([
      zWorkspace,
      t.type({
        tabs: t.array(zWorkspaceBackupTab),
      }),
    ]),
  ),
});

export const zWorkspacesServicesBackup = t.type({
  workspaces: t.array(zWorkspaceBackup),
  currentWorkspaceId: t.string,
  timestamp: t.number,
});

export const zWorkspacesServicesConfigs = t.type({
  manageOnBms: t.boolean,
  showWorkspaceNameOnToolbar: t.boolean,
  closePopupAfterClick: t.boolean,
});

/* Export as types */
export type TWorkspaceID = t.TypeOf<typeof zWorkspaceID>;
export type TWorkspace = t.TypeOf<typeof zWorkspace>;
export type TWorkspacesStoreData = t.TypeOf<
  typeof zWorkspacesServicesStoreData
>;
export type TWorkspaceBackupTab = t.TypeOf<typeof zWorkspaceBackupTab>;
export type TWorkspaceBackup = t.TypeOf<typeof zWorkspaceBackup>;
export type TWorkspacesBackup = t.TypeOf<typeof zWorkspacesServicesBackup>;
export type TWorkspacesServicesConfigs = t.TypeOf<
  typeof zWorkspacesServicesConfigs
>;

/* XUL Elements */
export type PanelMultiViewParentElement = XULElement & {
  hidePopup: () => void;
};
