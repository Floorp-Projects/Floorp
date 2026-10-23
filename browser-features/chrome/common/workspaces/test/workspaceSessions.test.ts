// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  getWorkspaceSessionIdentities,
  getWorkspaceSessionName,
  isValidWorkspaceSession,
  releaseUnusedWorkspaceSession,
  resolveWorkspaceSessionSelection,
  WorkspaceSessionError,
} from "../utils/workspace-sessions.ts";
import type {
  WorkspaceSessionErrorReason,
  WorkspaceSessionIdentity,
  WorkspaceSessionIdentityService,
} from "../utils/workspace-session-types.ts";

class TestIdentityService implements WorkspaceSessionIdentityService {
  identities: WorkspaceSessionIdentity[] = [
    { userContextId: 2, name: "Work", public: true },
    {
      userContextId: 3,
      name: "Private",
      public: true,
      floorpPrivateContainer: true,
    },
  ];
  readonly created: string[] = [];
  readonly removed: number[] = [];
  failCreate = false;

  ensureDataReady(): void {}

  getPublicIdentities(): WorkspaceSessionIdentity[] {
    return this.identities;
  }

  getUserContextLabel(userContextId: number): string {
    return this.identities.find((item) => item.userContextId === userContextId)
      ?.name ?? "";
  }

  create(name: string): WorkspaceSessionIdentity {
    if (this.failCreate) throw new Error("Creation failed");
    const identity = {
      userContextId: 10 + this.created.length,
      name,
      public: true,
    };
    this.created.push(name);
    this.identities.push(identity);
    return identity;
  }

  remove(userContextId: number): boolean {
    this.removed.push(userContextId);
    this.identities = this.identities.filter((item) =>
      item.userContextId !== userContextId
    );
    return true;
  }
}

function assertSessionError(
  fn: () => unknown,
  reason: WorkspaceSessionErrorReason,
): void {
  const error = assertThrows(fn, `Expected session error: ${reason}`);
  assert(error instanceof WorkspaceSessionError, "Errors retain their reason");
  assertEquals(error.reason, reason, "Expected session failure reason");
}

const tests: TestCase[] = [
  {
    name: "Private containers are excluded even when public",
    fn() {
      const service = new TestIdentityService();
      assertEquals(
        getWorkspaceSessionIdentities(service).map((item) => item.userContextId)
          .join(","),
        "2",
        "Only normal identities are listed",
      );
      assert(
        !isValidWorkspaceSession(3, service),
        "Private ID cannot be chosen",
      );
      assertSessionError(
        () => resolveWorkspaceSessionSelection("3", "Project", service),
        "missing",
      );
    },
  },
  {
    name: "Sharing standard or existing sessions creates no container",
    fn() {
      const service = new TestIdentityService();
      const standard = resolveWorkspaceSessionSelection(
        "0",
        "Project",
        service,
      );
      const shared = resolveWorkspaceSessionSelection("2", "Project", service);
      assertEquals(standard.userContextId, 0, "Explicit standard remains 0");
      assertEquals(standard.created, false, "Standard is never created");
      assertEquals(shared.userContextId, 2, "The chosen ID is shared");
      assertEquals(
        shared.created,
        false,
        "Sharing does not create a duplicate",
      );
      assertEquals(
        service.created.length,
        0,
        "No empty containers are created",
      );
      assertEquals(
        getWorkspaceSessionName(2, service),
        "Work",
        "Name is shared",
      );
    },
  },
  {
    name: "Missing selections do not become standard through coercion",
    fn() {
      const service = new TestIdentityService();
      for (
        const selection of [
          "",
          " ",
          "0.0",
          "2e0",
          "-1",
          "02",
          null,
          undefined,
          -1,
          0.5,
          Number.NaN,
          {},
        ]
      ) {
        assertSessionError(
          () => resolveWorkspaceSessionSelection(selection, "Project", service),
          "invalid",
        );
      }
      assertSessionError(
        () => resolveWorkspaceSessionSelection("9", "Project", service),
        "missing",
      );
      assertEquals(service.created.length, 0, "Invalid choices create nothing");
    },
  },
  {
    name: "Revalidate a shared ID after it is removed",
    fn() {
      const service = new TestIdentityService();
      assert(isValidWorkspaceSession(2, service), "Shared choice starts valid");
      service.remove(2);
      assertSessionError(
        () => resolveWorkspaceSessionSelection("2", "Project", service),
        "missing",
      );
      assertEquals(getWorkspaceSessionName(2, service), null, "No stale name");
    },
  },
  {
    name: "Only explicit new choices create a named normal session",
    fn() {
      const service = new TestIdentityService();
      const session = resolveWorkspaceSessionSelection(
        "new",
        "Client",
        service,
      );
      assertEquals(session.userContextId, 10, "New identity is returned");
      assertEquals(session.created, true, "Ownership is recorded for rollback");
      assertEquals(service.created.join(","), "Client", "Uses workspace name");
      assert(isValidWorkspaceSession(10, service), "New session is selectable");
      assertSessionError(
        () => resolveWorkspaceSessionSelection("new", "  ", service),
        "invalid",
      );
      assertEquals(service.created.length, 1, "Blank names create nothing");
    },
  },
  {
    name: "Creation failures are reported instead of falling back",
    fn() {
      const service = new TestIdentityService();
      service.failCreate = true;
      assertSessionError(
        () => resolveWorkspaceSessionSelection("new", "Client", service),
        "create",
      );
      assertEquals(
        service.created.length,
        0,
        "No standard fallback is created",
      );
    },
  },
  {
    name: "Rollback keeps existing and newly referenced sessions",
    fn() {
      const service = new TestIdentityService();
      releaseUnusedWorkspaceSession(
        { userContextId: 2, created: false },
        () => false,
        service,
      );
      const first = resolveWorkspaceSessionSelection(
        "new",
        "Client A",
        service,
      );
      releaseUnusedWorkspaceSession(first, () => true, service);
      assertEquals(service.removed.length, 0, "Shared sessions are preserved");
      const second = resolveWorkspaceSessionSelection(
        "new",
        "Client B",
        service,
      );
      releaseUnusedWorkspaceSession(second, () => false, service);
      assertEquals(
        service.removed.join(","),
        String(second.userContextId),
        "Only this operation's unreferenced session is removed",
      );
      assert(
        isValidWorkspaceSession(first.userContextId, service),
        "A stays valid",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSessions.test.ts", tests);
}
