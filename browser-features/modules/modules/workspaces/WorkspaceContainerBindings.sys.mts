// SPDX-License-Identifier: MPL-2.0

import {
  type LocalWorkspaceContainer,
  WORKSPACE_CONTAINER_BINDINGS_CHANGED,
  WorkspaceContainerBindingCoordinator,
} from "../../common/workspace-container-bindings.ts";
import type { WorkspaceContainerDefinition } from "../../common/workspace-container-types.ts";

export type {
  LocalWorkspaceContainer,
  WorkspaceContainerPendingReason,
  WorkspaceContainerResolution,
} from "../../common/workspace-container-bindings.ts";
export { WORKSPACE_CONTAINER_BINDINGS_CHANGED };

interface NativeContainerIdentity {
  userContextId: number;
  name: string;
  color: string;
  icon: string;
  public: boolean;
  l10nId?: string;
}

interface NativeIdentityService {
  getPublicIdentities(): NativeContainerIdentity[];
  getUserContextLabel(id: number): string;
  create(name: string, icon: string, color: string): NativeContainerIdentity;
  save(): Promise<void>;
  readonly containerColors: string[];
  readonly containerIcons: string[];
}

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
) as { ContextualIdentityService: NativeIdentityService };

const STORAGE_PATH = PathUtils.join(
  PathUtils.profileDir,
  "floorp-workspace-container-bindings.json",
);

function listIdentities(): LocalWorkspaceContainer[] {
  return ContextualIdentityService.getPublicIdentities().filter((identity) =>
    identity.public && Number.isSafeInteger(identity.userContextId) &&
    identity.userContextId > 0
  ).map((identity) => ({
    userContextId: identity.userContextId,
    name: ContextualIdentityService.getUserContextLabel(identity.userContextId),
    color: identity.color,
    icon: identity.icon,
  }));
}

/** Does not read cookies, credentials, origin storage, or extension settings. */
export const WorkspaceContainerBindings =
  new WorkspaceContainerBindingCoordinator({
    async load() {
      try {
        return await IOUtils.readJSON(STORAGE_PATH);
      } catch (error) {
        if (error instanceof DOMException && error.name === "NotFoundError") {
          return null;
        }
        throw error;
      }
    },
    async save(state) {
      // JSONFile.saveSoon/_save cannot attest this boundary: _save handles its
      // own write errors. IOUtils propagates failures before native deletion.
      await IOUtils.writeJSON(STORAGE_PATH, state, {
        tmpPath: `${STORAGE_PATH}.tmp`,
      });
    },
    list: listIdentities,
    create(definition: WorkspaceContainerDefinition) {
      const identity = ContextualIdentityService.create(
        definition.name,
        definition.icon,
        definition.color,
      );
      return {
        userContextId: identity.userContextId,
        name: definition.name,
        icon: identity.icon,
        color: identity.color,
      };
    },
    persistIdentities: () => ContextualIdentityService.save(),
    supports: (definition) =>
      Boolean(
        definition.name.trim() &&
          ContextualIdentityService.containerColors.includes(
            definition.color,
          ) &&
          ContextualIdentityService.containerIcons.includes(definition.icon),
      ),
    enabled: () =>
      Services.prefs.getBoolPref("privacy.userContext.enabled", false),
    uuid: () => Services.uuid.generateUUID().toString().slice(1, -1),
    now: () => Date.now(),
    changed: () =>
      Services.obs.notifyObservers(
        null as unknown as nsISupports,
        WORKSPACE_CONTAINER_BINDINGS_CHANGED,
      ),
  });

export type WorkspaceContainerBindingsAPI = typeof WorkspaceContainerBindings;

interface RemovalOperation {
  operationId: string;
  userContextId: number;
  userContextIds?: number[];
  reason: "delete" | "disable";
  outcome?: "completed" | "cancelled";
  blockers: Promise<void>[];
  prepared?: boolean;
  blocked?: boolean;
}

function wrapped(subject: nsISupports | null): unknown {
  return (subject as { wrappedJSObject?: unknown } | null)?.wrappedJSObject;
}

function logPersistenceFailure(error: unknown): void {
  console.error(
    "[WorkspaceContainerBindings] Could not persist container state",
    error,
  );
}

Services.obs.addObserver((subject) => {
  const identity = wrapped(subject) as
    | Partial<NativeContainerIdentity>
    | undefined;
  if (typeof identity?.userContextId !== "number") return;
  WorkspaceContainerBindings.noteCreated(identity.userContextId);
  if (WorkspaceContainerBindings.isReady()) {
    WorkspaceContainerBindings.flush().catch(logPersistenceFailure);
  }
}, "contextual-identity-created");

Services.obs.addObserver((subject) => {
  const identity = wrapped(subject) as
    | Partial<NativeContainerIdentity>
    | undefined;
  if (typeof identity?.userContextId !== "number") return;
  WorkspaceContainerBindings.markDeleted([identity.userContextId]);
  if (WorkspaceContainerBindings.isReady()) {
    WorkspaceContainerBindings.flush().catch(logPersistenceFailure);
  }
}, "contextual-identity-deleted");

Services.obs.addObserver((subject) => {
  const operation = wrapped(subject) as RemovalOperation | undefined;
  if (!operation || !Array.isArray(operation.blockers)) return;
  operation.blockers.push(WorkspaceContainerBindings.prepareRemoval(
    operation.operationId,
    operation.userContextId,
    operation.reason,
    operation.userContextIds,
  ));
}, "floorp-container-removal-preparing");

Services.obs.addObserver((subject) => {
  const operation = wrapped(subject) as RemovalOperation | undefined;
  if (
    !operation || operation.prepared ||
    WorkspaceContainerBindings.isReady() &&
      !WorkspaceContainerBindings.hasBinding(operation.userContextId)
  ) return;
  // A synchronous direct remove must not bypass the async preflight.
  operation.blocked = true;
}, "floorp-container-removal-guard");

Services.obs.addObserver((subject) => {
  const operation = wrapped(subject) as RemovalOperation | undefined;
  if (
    !operation || !Array.isArray(operation.blockers) ||
    (operation.outcome !== "completed" && operation.outcome !== "cancelled")
  ) return;
  operation.blockers.push(WorkspaceContainerBindings.finishRemoval(
    operation.operationId,
    operation.outcome,
  ));
}, "floorp-container-removal-finished");

Services.prefs.addObserver("privacy.userContext.enabled", () => {
  WorkspaceContainerBindings.resumeDisabled().catch(logPersistenceFailure);
});

Services.obs.addObserver(() => {
  // The sync engine owns the single durable reset. Invalidate immediately so
  // prompts/native create continuations cannot outlive its first async wait.
  WorkspaceContainerBindings.invalidateAccount();
}, "weave:service:start-over");
