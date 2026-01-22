/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Workspace API routes for the OS Server
 *
 * Provides workspace management capabilities:
 * - GET  /workspaces         -> List all workspaces
 * - GET  /workspaces/current -> Get current workspace ID
 * - POST /workspaces/:id/switch -> Switch to specific workspace
 * - POST /workspaces/next    -> Switch to next workspace
 * - POST /workspaces/previous -> Switch to previous workspace
 */

import type {
  NamespaceBuilder,
  Context as RouterContext,
} from "../router.sys.mts";
import type { WorkspacesApiModule } from "./types.ts";
import type { ErrorResponse } from "../_os-plugin/api-spec/types.ts";

// Lazy import of WorkspacesApiService module
const WorkspacesApiModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/workspaces/WorkspacesApiService.sys.mjs",
  ) as WorkspacesApiModule;

// -- Route registration -------------------------------------------------------

export function registerWorkspaceRoutes(api: NamespaceBuilder): void {
  api.namespace("/workspaces", (w: NamespaceBuilder) => {
    // List all workspaces
    w.get("/", () => {
      const { WorkspacesApiService } = WorkspacesApiModule();
      const workspaces = WorkspacesApiService.listWorkspaces();
      const currentId = WorkspacesApiService.getCurrentWorkspaceId();
      return {
        status: 200,
        body: {
          workspaces,
          currentId,
        },
      };
    });

    // Get current workspace ID
    w.get("/current", () => {
      const { WorkspacesApiService } = WorkspacesApiModule();
      const currentId = WorkspacesApiService.getCurrentWorkspaceId();
      return {
        status: 200,
        body: { workspaceId: currentId },
      };
    });

    // Switch to next workspace
    w.post<
      undefined,
      { success: boolean; workspaceId: string | null } | ErrorResponse
    >("/next", () => {
      const { WorkspacesApiService } = WorkspacesApiModule();
      const success = WorkspacesApiService.switchToNext();
      if (!success) {
        return {
          status: 500,
          body: { error: "Failed to switch to next workspace" },
        };
      }
      const currentId = WorkspacesApiService.getCurrentWorkspaceId();
      return {
        status: 200,
        body: { success: true, workspaceId: currentId },
      };
    });

    // Switch to previous workspace
    w.post<
      undefined,
      { success: boolean; workspaceId: string | null } | ErrorResponse
    >("/previous", () => {
      const { WorkspacesApiService } = WorkspacesApiModule();
      const success = WorkspacesApiService.switchToPrevious();
      if (!success) {
        return {
          status: 500,
          body: { error: "Failed to switch to previous workspace" },
        };
      }
      const currentId = WorkspacesApiService.getCurrentWorkspaceId();
      return {
        status: 200,
        body: { success: true, workspaceId: currentId },
      };
    });

    // Switch to specific workspace by ID
    w.post<
      undefined,
      { success: boolean; workspaceId: string | null } | ErrorResponse
    >("/:id/switch", (ctx: RouterContext) => {
      const { id } = ctx.params;
      if (!id) {
        return {
          status: 400,
          body: { error: "Workspace ID is required" } as ErrorResponse,
        };
      }

      const { WorkspacesApiService } = WorkspacesApiModule();
      const success = WorkspacesApiService.switchToWorkspace(id);
      if (!success) {
        return {
          status: 404,
          body: { error: `Workspace not found: ${id}` } as ErrorResponse,
        };
      }
      return {
        status: 200,
        body: { success: true, workspaceId: id },
      };
    });
  });
}
