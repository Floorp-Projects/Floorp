// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  type LocalWorkspaceContainer,
  WorkspaceContainerBindingCoordinator,
  type WorkspaceContainerBindingState,
} from "../../common/workspace-container-bindings.ts";
import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "../../common/workspace-container-types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

const device = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const containerId = "11111111-1111-4111-8111-111111111111";
const anotherId = "22222222-2222-4222-8222-222222222222";
const operationId = "33333333-3333-4333-8333-333333333333";
const ref: WorkspaceContainerRef = { kind: "container", containerId };
const definition = (
  id = containerId,
  name = "Work",
): WorkspaceContainerDefinition => ({
  version: 2,
  kind: "container",
  containerId: id,
  name,
  icon: "briefcase",
  color: "blue",
  modifiedAt: 1,
  modifiedBy: device,
});
const identity = (
  userContextId: number,
  name = "Work",
): LocalWorkspaceContainer => ({
  userContextId,
  name,
  icon: "briefcase",
  color: "blue",
});

async function rejects(
  operation: Promise<unknown>,
  message: string,
): Promise<void> {
  let rejected = false;
  try {
    await operation;
  } catch {
    rejected = true;
  }
  assert(rejected, message);
}

function fixture(initial: LocalWorkspaceContainer[] = [], persisted?: unknown) {
  const state = {
    identities: initial.map((value) => ({ ...value })),
    persisted: persisted ?? null as unknown,
    saved: 0,
    created: 0,
    notifications: [] as string[],
    associationWasReady: [] as boolean[],
    failSave: false,
    enabled: true,
    beforeLoad: (): Promise<void> => Promise.resolve(),
    beforeSave: (): Promise<void> => Promise.resolve(),
    persistIdentity: (): Promise<void> => Promise.resolve(),
  };
  let counter = 100;
  let coordinator: WorkspaceContainerBindingCoordinator;
  const build = () => {
    coordinator = new WorkspaceContainerBindingCoordinator({
      load: async () => {
        await state.beforeLoad();
        return structuredClone(state.persisted);
      },
      save: async (value) => {
        await state.beforeSave();
        if (state.failSave) {
          return Promise.reject(new Error("Fixture write failure"));
        }
        state.persisted = structuredClone(value);
        state.saved++;
        return Promise.resolve();
      },
      list: () => state.identities.map((value) => ({ ...value })),
      create: (value) => {
        const next = identity(
          Math.max(0, ...state.identities.map((item) => item.userContextId)) +
            1,
          value.name,
        );
        next.color = value.color;
        next.icon = value.icon;
        state.identities.push(next);
        state.created++;
        coordinator.noteCreated(next.userContextId);
        return { ...next };
      },
      persistIdentities: () => state.persistIdentity(),
      supports: (value) =>
        value.color === "blue" && value.icon === "briefcase" &&
        Boolean(value.name.trim()),
      enabled: () => state.enabled,
      uuid: () =>
        `${
          (++counter).toString(16).padStart(8, "0")
        }-0000-4000-8000-000000000000`,
      now: () => 50,
      changed() {
        state.notifications.push("changed");
      },
      associationChanged(id) {
        state.notifications.push(`association:${id}`);
        state.associationWasReady.push(
          coordinator.getState(id).status === "resolved",
        );
      },
    });
    return coordinator;
  };
  return { state, service: build(), restart: build };
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceContainerBindings.test.mts", [
    {
      name:
        "association notifications precede general changes and exclude source capture or rename",
      fn: async () => {
        const f = fixture([identity(7), identity(9)]);
        await f.service.ready("account", device);
        const captured = f.service.captureReference(7);
        assert(
          captured.ref.kind === "container",
          "source capture creates a logical definition",
        );
        f.state.identities[0].name = "Local rename";
        f.service.captureReference(7, captured.ref);
        await f.service.receiveDefinitions([
          definition(anotherId, "New native target"),
        ]);
        assert(
          !f.state.notifications.some((item) =>
            item.startsWith("association:")
          ),
          "source capture, rename and metadata receive do not announce reassociation",
        );
        f.state.notifications.length = 0;
        await f.service.bind(captured.ref.containerId, 9);
        assertEquals(
          f.state.notifications[0],
          `association:${captured.ref.containerId}`,
          "specific association is announced first",
        );
        assertEquals(
          f.state.notifications[1],
          "changed",
          "general capture signal follows association",
        );
        f.state.notifications.length = 0;
        await f.service.resolve({ kind: "container", containerId: anotherId });
        const associated = f.state.notifications.indexOf(
          `association:${anotherId}`,
        );
        assert(
          associated >= 0 &&
            f.state.notifications[associated + 1] === "changed",
          "successful native creation announces association before its general change",
        );
        assert(
          f.state.associationWasReady.every(Boolean),
          "listeners see the newly assigned mapping synchronously",
        );
        f.state.notifications.length = 0;
        f.state.failSave = true;
        await rejects(
          f.service.bind(captured.ref.containerId, 7),
          "binding save failure propagates",
        );
        assertEquals(
          f.state.notifications[0],
          `association:${captured.ref.containerId}`,
          "association protection was already announced before the failed save",
        );
      },
    },
    {
      name:
        "a same-name existing identity waits for explicit binding and keeps its presentation",
      fn: async () => {
        const f = fixture([{
          ...identity(7),
          color: "red",
          icon: "fingerprint",
        }]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        const pending = await f.service.resolve(ref);
        assert(
          pending.status === "pending" && pending.reason === "confirmation",
          "first match needs confirmation",
        );
        assertEquals(
          pending.candidates[0].userContextId,
          7,
          "uses local candidate ID",
        );
        const bound = await f.service.bind(containerId, 7);
        assert(bound.status === "resolved", "explicit binding resolves");
        assertEquals(bound.userContextId, 7, "receiver ID is used");
        assertEquals(
          f.state.identities[0].color,
          "red",
          "local presentation unchanged",
        );
        assertEquals(f.state.created, 0, "reuse does not create");
      },
    },
    {
      name: "duplicate names require selection, and matching remains exact",
      fn: async () => {
        const f = fixture([identity(2), identity(5)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        const pending = await f.service.resolve(ref);
        assert(pending.status === "pending", "duplicate candidates held");
        assertEquals(
          pending.candidates.length,
          2,
          "all exact candidates offered",
        );
        await f.service.bind(containerId, 5);
        const same = await f.service.resolve(ref);
        assert(
          same.status === "resolved" && same.userContextId === 5,
          "selected candidate remembered",
        );
        await f.service.receiveDefinitions([definition(anotherId, "work")]);
        const created = await f.service.resolve({
          kind: "container",
          containerId: anotherId,
        });
        assert(
          created.status === "resolved",
          "different case creates a separate identity",
        );
        assertEquals(f.state.created, 1, "no fuzzy merge");
      },
    },
    {
      name:
        "parallel receives and repeated explicit clicks create one native identity",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        const [a, b] = await Promise.all([
          f.service.resolve(ref),
          f.service.resolve(ref),
        ]);
        assert(
          a.status === "resolved" && b.status === "resolved",
          "both receives resolve",
        );
        assertEquals(a.userContextId, b.userContextId, "one shared binding");
        await Promise.all([
          f.service.create(containerId),
          f.service.create(containerId),
        ]);
        assertEquals(f.state.created, 1, "repeated click does not duplicate");
        const restarted = f.restart();
        await restarted.ready("account", device);
        const c = await restarted.resolve(ref);
        assert(
          c.status === "resolved" && c.userContextId === a.userContextId,
          "binding survives restart",
        );
        assertEquals(f.state.created, 1, "restart does not duplicate");
      },
    },
    {
      name:
        "different logical IDs with the same name cannot auto-create concurrently",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([
          definition(),
          definition(anotherId),
        ]);
        const [first, second] = await Promise.all([
          f.service.resolve(ref),
          f.service.resolve({ kind: "container", containerId: anotherId }),
        ]);
        assert(first.status === "resolved", "first definition created");
        assert(
          second.status === "pending" && second.reason === "confirmation",
          "second sees the newly existing candidate",
        );
        assertEquals(f.state.created, 1, "profile-wide creation lock");
      },
    },
    {
      name:
        "local rename keeps remembered instance and immutable creation metadata",
      fn: async () => {
        const f = fixture([identity(9)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 9);
        f.state.identities[0].name = "Renamed locally";
        const resolved = await f.service.resolve(ref);
        assert(
          resolved.status === "resolved" && resolved.userContextId === 9,
          "rename does not detach",
        );
        const capture = f.service.captureReference(9, ref);
        assert(
          capture.ref.kind === "container" &&
            capture.ref.containerId === containerId,
          "preferred logical ID kept",
        );
        assertEquals(
          capture.definition?.name,
          "Work",
          "local rename is not a global metadata edit",
        );
      },
    },
    {
      name: "deleted targets do not auto-create or bind a reused numeric ID",
      fn: async () => {
        const f = fixture([identity(4)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 4);
        f.state.identities = [];
        f.service.markDeleted([4]);
        await f.service.flush();
        f.state.identities = [identity(4)];
        f.service.noteCreated(4);
        const deleted = await f.service.resolve(ref);
        assert(
          deleted.status === "pending" && deleted.reason === "deleted",
          "new lifetime cannot revive binding",
        );
        assertEquals(
          f.state.created,
          0,
          "no automatic recreation after deletion",
        );
        const restored = await f.service.bind(containerId, 4);
        assert(restored.status === "resolved", "explicit rebind works");
      },
    },
    {
      name: "startup detects a missing identity before it can be used",
      fn: async () => {
        const f = fixture([identity(3)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 3);
        f.state.identities = [];
        const restarted = f.restart();
        await restarted.ready("account", device);
        const result = await restarted.resolve(ref);
        assert(
          result.status === "pending" && result.reason === "deleted",
          "missing target stays deleted across startup",
        );
        assertEquals(
          f.state.created,
          0,
          "startup did not restore deliberately deleted container",
        );
      },
    },
    {
      name:
        "removal freezes resolution immediately and cancellation restores it",
      fn: async () => {
        const f = fixture([identity(3)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 3);
        const prepared = f.service.prepareRemoval(operationId, 3, "delete");
        const frozen = f.service.lookup(ref);
        assert(
          frozen.status === "pending" && frozen.reason === "deleting",
          "frozen before asynchronous persistence",
        );
        await prepared;
        await f.service.finishRemoval(operationId, "cancelled");
        assert(
          f.service.lookup(ref).status === "resolved",
          "cancellation restores old binding",
        );
      },
    },
    {
      name:
        "completed native disable invalidates targets even if defaults reuse their numbers",
      fn: async () => {
        const f = fixture([identity(1)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 1);
        await f.service.prepareRemoval(operationId, 0, "disable", [1]);
        f.state.enabled = false;
        f.state.identities = [identity(1, "Personal")];
        await f.service.finishRemoval(operationId, "completed");
        f.state.enabled = true;
        await f.service.resumeDisabled();
        const result = await f.service.resolve(ref);
        assert(
          result.status === "pending" && result.reason === "deleted",
          "default ID is not mistaken for the former identity",
        );
      },
    },
    {
      name:
        "re-enabling during native disable does not discard its removal freeze",
      fn: async () => {
        const f = fixture([identity(1)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 1);
        await f.service.prepareRemoval(operationId, 0, "disable", [1]);
        f.state.enabled = false;
        f.state.enabled = true;
        await f.service.resumeDisabled();
        const frozen = f.service.lookup(ref);
        assert(
          frozen.status === "pending" && frozen.reason === "disabled",
          "preflight remains frozen until native completion",
        );
        await f.service.finishRemoval(operationId, "cancelled");
        assert(
          f.service.lookup(ref).status === "resolved",
          "native cancellation releases freeze",
        );
      },
    },
    {
      name: "failure to durably prepare creation prevents native mutation",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        f.state.failSave = true;
        await rejects(
          f.service.resolve(ref),
          "write failure rejects before create",
        );
        assertEquals(f.state.created, 0, "native create never ran");
        const failed = f.service.lookup(ref);
        assert(
          failed.status === "pending" && failed.reason === "creation-failed",
          "failed write does not leave an endless creating state",
        );
        f.state.failSave = false;
        assert(
          (await f.service.create(containerId)).status === "resolved",
          "explicit retry recovers after storage is writable",
        );
      },
    },
    {
      name: "disable during journal persistence prevents native creation",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        let release!: () => void;
        let started!: () => void;
        const startedPromise = new Promise<void>((resolve) =>
          started = resolve
        );
        const gate = new Promise<void>((resolve) => release = resolve);
        f.state.beforeSave = () => {
          started();
          return gate;
        };
        const creating = f.service.resolve(ref);
        await startedPromise;
        const disabling = f.service.prepareRemoval(
          operationId,
          0,
          "disable",
          [],
        );
        f.state.enabled = false;
        release();
        const result = await creating;
        await disabling;
        assert(
          result.status === "pending" && result.reason === "disabled",
          "disabled creation is held",
        );
        assertEquals(
          f.state.created,
          0,
          "no identity escaped the native disable snapshot",
        );
        await f.service.finishRemoval(operationId, "cancelled");
        f.state.enabled = true;
        const retried = await f.service.resolve(ref);
        assert(
          retried.status === "resolved",
          "cancelled preflight allows normal first creation",
        );
      },
    },
    {
      name:
        "queued repeated clicks do not duplicate an uncertain native create",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        let failed = false;
        f.state.persistIdentity = () => {
          if (!failed) {
            failed = true;
            return Promise.reject(new Error("Interrupted native save"));
          }
          return Promise.resolve();
        };
        const [first, second] = await Promise.all([
          f.service.create(containerId),
          f.service.create(containerId),
        ]);
        assert(
          first.status === "pending" && first.reason === "creation-failed",
          "first native save failure held",
        );
        assert(
          second.status === "pending" && second.reason === "confirmation",
          "stale click requires another choice",
        );
        assertEquals(
          f.state.created,
          1,
          "one native create despite failed persistence",
        );
        await f.service.bind(containerId, f.state.identities[0].userContextId);
        assert(
          f.service.lookup(ref).status === "resolved",
          "existing uncertain identity can be confirmed",
        );
      },
    },
    {
      name: "failed final binding write retains its journal for queued clicks",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        let failed = false;
        f.state.beforeSave = () => {
          if (f.state.created && !failed) {
            failed = true;
            return Promise.reject(new Error("Final binding write failed"));
          }
          return Promise.resolve();
        };
        const [first, second] = await Promise.all([
          f.service.create(containerId),
          f.service.create(containerId),
        ]);
        assert(
          first.status === "pending" && first.reason === "creation-failed",
          "final persistence failure held",
        );
        assert(
          second.status === "pending" && second.reason === "confirmation",
          "stale click cannot repeat uncertain create",
        );
        assertEquals(f.state.created, 1, "only one native identity exists");
        const saved = f.state.persisted as WorkspaceContainerBindingState;
        assert(
          Boolean(saved.creations[containerId]),
          "journal restored after unsuccessful final binding commit",
        );
      },
    },
    {
      name:
        "an interrupted create becomes confirmation after restart instead of creating again",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        f.state.persistIdentity = () =>
          Promise.reject(new Error("Interrupted native save"));
        const failed = await f.service.resolve(ref);
        assert(
          failed.status === "pending" && failed.reason === "creation-failed",
          "native save failure held",
        );
        assertEquals(
          f.state.created,
          1,
          "native creation may already have occurred",
        );
        const restarted = f.restart();
        await restarted.ready("account", device);
        const recovered = await restarted.resolve(ref);
        assert(
          recovered.status === "pending" && recovered.reason === "confirmation",
          "uncertain ownership asks for selection",
        );
        assertEquals(f.state.created, 1, "no duplicate recovery creation");
      },
    },
    {
      name:
        "account change fences an in-flight native creation and retains local identity",
      fn: async () => {
        const f = fixture();
        await f.service.ready("first", device);
        await f.service.receiveDefinitions([definition()]);
        let release!: () => void;
        let started!: () => void;
        const startedPromise = new Promise<void>((resolve) =>
          started = resolve
        );
        const gate = new Promise<void>((resolve) => release = resolve);
        f.state.persistIdentity = () => {
          started();
          return gate;
        };
        const opening = f.service.resolve(ref);
        await startedPromise;
        const switching = f.service.ready("second", device);
        assert(
          !f.service.isReady(),
          "old bindings are unavailable immediately",
        );
        release();
        await rejects(opening, "old creation completion rejected");
        await switching;
        assertEquals(
          f.service.listDefinitions().length,
          0,
          "new account does not inherit old definitions",
        );
        assertEquals(
          f.state.identities.length,
          1,
          "account reset does not delete native containers",
        );
      },
    },
    {
      name:
        "anonymous capture adopts first account but sign-out clears associations",
      fn: async () => {
        const f = fixture([identity(5)]);
        await f.service.ready(null, device);
        const captured = f.service.captureReference(5);
        await f.service.flush();
        await f.service.ready("first", device);
        assert(
          f.service.lookup(captured.ref).status === "resolved",
          "first account adopts unpublished local definition",
        );
        await f.service.resetAccount();
        await f.service.ready("second", device);
        assertEquals(
          f.service.listDefinitions().length,
          0,
          "reset prevents old-account inheritance",
        );
        assertEquals(
          f.state.identities.length,
          1,
          "existing container remains",
        );
      },
    },
    {
      name: "missing and legacy references never resolve as explicit none",
      fn: async () => {
        const f = fixture();
        await f.service.ready("account", device);
        const absent = await f.service.resolve(ref);
        assert(
          absent.status === "pending" && absent.reason === "definition-missing",
          "definition not received is held",
        );
        const legacy = await f.service.resolve({ kind: "legacy-unresolved" });
        assert(
          legacy.status === "pending" && legacy.reason === "legacy-unresolved",
          "legacy belongs in migration UI",
        );
        const none = await f.service.resolve({ kind: "none" });
        assert(
          none.status === "resolved" && none.userContextId === 0,
          "explicit none remains valid",
        );
        assertEquals(f.state.created, 0, "no inappropriate native creation");
      },
    },
    {
      name:
        "interrupted removal survives restart as recoverable reassociation, including reused default IDs",
      fn: async () => {
        const f = fixture([identity(1)]);
        await f.service.ready("account", device);
        await f.service.receiveDefinitions([definition()]);
        await f.service.bind(containerId, 1);
        await f.service.prepareRemoval(operationId, 0, "disable", [1]);
        f.state.identities = [identity(1, "New default")];
        const restarted = f.restart();
        await restarted.ready("account", device);
        await restarted.resumeDisabled();
        const result = await restarted.resolve(ref);
        assert(
          result.status === "pending" && result.reason === "deleted",
          "interrupted reset cannot revive stale mapping",
        );
        const restored = await restarted.bind(containerId, 1);
        assert(
          restored.status === "resolved",
          "explicit recovery remains available",
        );
      },
    },
    {
      name:
        "sign-out during initial load fences old readiness until durable reset",
      fn: async () => {
        const f = fixture([identity(1)]);
        await f.service.ready("first", device);
        f.service.captureReference(1);
        await f.service.flush();
        let release!: () => void;
        let started!: () => void;
        const startedPromise = new Promise<void>((resolve) =>
          started = resolve
        );
        const gate = new Promise<void>((resolve) => release = resolve);
        f.state.beforeLoad = () => {
          started();
          return gate;
        };
        const restarted = f.restart();
        const oldReady = restarted.ready("first", device);
        await startedPromise;
        restarted.invalidateAccount();
        assert(
          !restarted.isReady(),
          "old account is unavailable synchronously",
        );
        await rejects(
          restarted.ready("first", device),
          "late old readiness cannot re-adopt the invalidated account",
        );
        const resetting = restarted.resetAccount();
        // Gecko observer order is unspecified: the wrapper may invalidate only
        // after the owning engine has already queued the same durable reset.
        restarted.invalidateAccount();
        release();
        await rejects(oldReady, "pending old account load is fenced");
        await resetting;
        await restarted.ready("second", device);
        assertEquals(
          restarted.listDefinitions().length,
          0,
          "new account inherits no unpublished old-account definitions",
        );
        assertEquals(
          f.state.identities.length,
          1,
          "reset preserves the native local container",
        );
      },
    },
    {
      name:
        "old prompt decisions cannot bind after account changes or release a new prompt lease",
      fn: async () => {
        const f = fixture([identity(1)]);
        await f.service.ready("first", device);
        await f.service.receiveDefinitions([definition()]);
        const oldEpoch = f.service.getEpoch();
        assert(f.service.acquirePrompt(), "old prompt acquired");
        await f.service.ready("second", device);
        await f.service.receiveDefinitions([definition()]);
        assert(f.service.acquirePrompt(), "new prompt acquired");
        await rejects(
          f.service.bind(containerId, 1, oldEpoch),
          "old UI choice rejected even when same UUID exists",
        );
        f.service.releasePrompt(oldEpoch);
        assert(
          !f.service.acquirePrompt(),
          "old finally did not release the new prompt",
        );
        f.service.releasePrompt(f.service.getEpoch());
        assert(f.service.acquirePrompt(), "current owner can release");
      },
    },
    {
      name:
        "corrupted persisted bindings fail closed without overwriting state",
      fn: async () => {
        const f = fixture([], { version: 99 });
        await rejects(
          f.service.ready("account", device),
          "unknown storage version rejected",
        );
        assertEquals(f.state.saved, 0, "invalid file was not replaced");
        assertEquals(
          f.state.created,
          0,
          "invalid state did not create containers",
        );
      },
    },
    {
      name: "binding state remains local and contains no site data",
      fn: async () => {
        const f = fixture([identity(5)]);
        await f.service.ready("account", device);
        f.service.captureReference(5);
        await f.service.flush();
        const saved = f.state.persisted as WorkspaceContainerBindingState;
        assertEquals(
          Object.keys(saved.definitions).length,
          1,
          "one definition saved",
        );
        const wire = f.service.listDefinitions()[0] as unknown as Record<
          string,
          unknown
        >;
        assert(
          !Object.hasOwn(wire, "userContextId"),
          "local numeric ID is not in definition",
        );
        assert(
          !Object.hasOwn(wire, "accountUid"),
          "account identity is not in definition",
        );
        assert(
          !Object.hasOwn(wire, "cookies"),
          "cookies are not part of binding transport",
        );
        assert(f.service.acquirePrompt(), "first window acquires prompt");
        assert(
          !f.service.acquirePrompt(),
          "second window cannot duplicate prompt",
        );
        f.service.releasePrompt();
        assert(f.service.acquirePrompt(), "prompt ownership released");
      },
    },
  ]);
}
