/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID, TWorkspaceSnapshot } from "./type.ts";

export interface WorkspaceArchiveFile {
  version: 1;
  snapshot: TWorkspaceSnapshot;
}

export interface WorkspaceArchiveSummary {
  archiveId: string;
  workspaceId: TWorkspaceID;
  name: string;
  icon: string | null;
  userContextId: number;
  capturedAt: number;
  filePath: string;
}
