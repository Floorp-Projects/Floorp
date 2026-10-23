// SPDX-License-Identifier: MPL-2.0

import type {
  ResolvedWorkspaceSession,
  WorkspaceSessionErrorReason,
  WorkspaceSessionIdentity,
  WorkspaceSessionIdentityService,
} from "./workspace-session-types.ts";

export const WORKSPACE_NEW_SESSION_VALUE = "new";

export class WorkspaceSessionError extends Error {
  constructor(readonly reason: WorkspaceSessionErrorReason) {
    super(`Workspace session ${reason}`);
    this.name = "WorkspaceSessionError";
  }
}

export const getWorkspaceSessionIdentityService =
  (): WorkspaceSessionIdentityService => {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
    ) as { ContextualIdentityService: WorkspaceSessionIdentityService };
    ContextualIdentityService.ensureDataReady();
    return ContextualIdentityService;
  };

export const getWorkspaceSessionIdentities = (
  service = getWorkspaceSessionIdentityService(),
): WorkspaceSessionIdentity[] => {
  service.ensureDataReady();
  return service.getPublicIdentities().filter((identity) =>
    !identity.floorpPrivateContainer &&
    Number.isInteger(identity.userContextId) && identity.userContextId > 0
  );
};

export const isValidWorkspaceSession = (
  userContextId: unknown,
  service = getWorkspaceSessionIdentityService(),
): userContextId is number => {
  if (
    typeof userContextId !== "number" || !Number.isSafeInteger(userContextId) ||
    userContextId < 0
  ) {
    return false;
  }
  return userContextId === 0 ||
    getWorkspaceSessionIdentities(service).some((identity) =>
      identity.userContextId === userContextId
    );
};

export const getWorkspaceSessionName = (
  userContextId: number,
  service = getWorkspaceSessionIdentityService(),
): string | null => {
  if (userContextId === 0 || !isValidWorkspaceSession(userContextId, service)) {
    return null;
  }
  return service.getUserContextLabel(userContextId);
};

/** Resolve a submitted choice immediately before persisting a workspace. */
export const resolveWorkspaceSessionSelection = (
  selection: unknown,
  name: string,
  service = getWorkspaceSessionIdentityService(),
): ResolvedWorkspaceSession => {
  if (!name.trim()) {
    throw new WorkspaceSessionError("invalid");
  }
  if (selection === WORKSPACE_NEW_SESSION_VALUE) {
    let identity: WorkspaceSessionIdentity;
    try {
      identity = service.create(name, "fingerprint", "blue");
    } catch {
      throw new WorkspaceSessionError("create");
    }
    if (!isValidWorkspaceSession(identity.userContextId, service)) {
      throw new WorkspaceSessionError("missing");
    }
    return { userContextId: identity.userContextId, created: true };
  }

  // Empty values, exponential notation, fractions and coercible objects are
  // never the standard context. In particular, Number("") must not become 0.
  if (
    (typeof selection !== "number" && typeof selection !== "string") ||
    (typeof selection === "string" && !/^(0|[1-9]\d*)$/.test(selection))
  ) {
    throw new WorkspaceSessionError("invalid");
  }
  const userContextId = Number(selection);
  if (!Number.isSafeInteger(userContextId) || userContextId < 0) {
    throw new WorkspaceSessionError("invalid");
  }
  if (!isValidWorkspaceSession(userContextId, service)) {
    throw new WorkspaceSessionError("missing");
  }
  return { userContextId, created: false };
};

/** Never delete a pre-existing or still referenced container during rollback. */
export const releaseUnusedWorkspaceSession = (
  session: ResolvedWorkspaceSession,
  isReferenced: (userContextId: number) => boolean,
  service = getWorkspaceSessionIdentityService(),
): void => {
  if (session.created && !isReferenced(session.userContextId)) {
    service.remove(session.userContextId);
  }
};
