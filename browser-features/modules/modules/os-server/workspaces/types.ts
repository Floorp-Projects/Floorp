/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for Workspace API routes
 */

import type { WorkspacesApiServiceType } from "../../os-apis/workspaces/WorkspacesApiService.sys.mts";

export type { WorkspacesApiServiceType };

export interface WorkspacesApiModule {
  WorkspacesApiService: WorkspacesApiServiceType;
}
