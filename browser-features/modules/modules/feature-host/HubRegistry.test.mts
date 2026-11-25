// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type {
  HubClientConnection,
  HubClientDiscoveryAdapter,
  HubGenerationLease,
  HubHostClaim,
  HubLiveDocument,
} from "./HubRegistryTypes.ts";

type RegistryModule = typeof import("./HubRegistry.sys.mts");
const { HubRegistry, HubRegistryError, parseHubPageDescriptor } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/HubRegistry.sys.mjs",
  ) as RegistryModule;

function assert(condition: boolean, message = "assertion failed"): void {
  harnessAssert(condition, message);
}

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "values should match",
): void {
  harnessAssertEquals(actual, expected, message);
}

function errorMessage(error: unknown): string {
  return typeof error === "object" && error !== null && "message" in error
    ? String(error.message)
    : String(error);
}

function assertDescriptorErrorCode(
  json: string,
  featureId: string,
  expectedCode: string,
): void {
  try {
    parseHubPageDescriptor(json, featureId);
  } catch (error) {
    if (!(error instanceof HubRegistryError)) throw error;
    assertEquals(error.code, expectedCode);
    return;
  }
  assert(false, `expected descriptor error: ${expectedCode}`);
}

async function rejectionOf(promise: Promise<unknown>): Promise<unknown> {
  try {
    await promise;
    return null;
  } catch (error) {
    return error;
  }
}

const descriptor = (featureId: string, localId = "sample") =>
  JSON.stringify({
    localId,
    route: `/extensions/${featureId}/${localId}`,
    titleKey: "hotfixHub.sample.title",
    navigation: { labelKey: "hotfixHub.sample.navigation", order: 10 },
    view: {
      kind: "message",
      titleKey: "hotfixHub.sample.title",
      bodyKey: "hotfixHub.sample.body",
      params: { version: "hub-v1" },
    },
  });

class FakeConnection implements HubClientConnection {
  readonly clientId: string;
  readonly connectionId: string;
  readonly windowId: string;
  readonly messages: string[] = [];
  readonly #closedListeners = new Set<() => void>();
  closed = false;
  onSend: ((json: string) => void) | null = null;

  constructor(id: string, windowId = "window-1") {
    this.clientId = `client-${id}`;
    this.connectionId = `connection-${id}`;
    this.windowId = windowId;
  }

  isClosed(): boolean {
    return this.closed;
  }

  sendSnapshot(json: string): void {
    this.messages.push(json);
    this.onSend?.(json);
  }

  onClosed(callback: () => void): () => void {
    this.#closedListeners.add(callback);
    if (this.closed) callback();
    return () => this.#closedListeners.delete(callback);
  }

  close(): void {
    this.closed = true;
    for (const callback of [...this.#closedListeners]) callback();
  }
}

class FakeDocument implements HubLiveDocument {
  readonly documentId: string;
  readonly windowId: string;
  readonly #availableListeners = new Set<() => void>();
  readonly #closedListeners = new Set<() => void>();
  live = true;

  constructor(
    public connection: FakeConnection | null,
    id = connection?.clientId ?? "document",
  ) {
    this.documentId = id;
    this.windowId = connection?.windowId ?? "window-1";
  }

  isLive(): boolean {
    return this.live;
  }

  ensureParentClient(): HubClientConnection | null {
    return this.connection;
  }

  onParentClientAvailable(callback: () => void): () => void {
    this.#availableListeners.add(callback);
    return () => this.#availableListeners.delete(callback);
  }

  onClosed(callback: () => void): () => void {
    this.#closedListeners.add(callback);
    if (!this.live) callback();
    return () => this.#closedListeners.delete(callback);
  }

  replaceConnection(connection: FakeConnection): void {
    this.connection = connection;
    for (const callback of [...this.#availableListeners]) callback();
  }

  closeDocument(): void {
    this.live = false;
    for (const callback of [...this.#closedListeners]) callback();
  }
}

class FakeDiscovery implements HubClientDiscoveryAdapter {
  queriedWindows: readonly string[] = [];

  constructor(readonly documents: FakeDocument[]) {}

  discoverLiveHubDocuments(
    windowIds: readonly string[],
  ): readonly HubLiveDocument[] {
    this.queriedWindows = [...windowIds];
    return this.documents.filter((document) =>
      windowIds.includes(document.windowId)
    );
  }
}

function ackJson(connection: FakeConnection): string {
  const message = JSON.parse(connection.messages.at(-1) ?? "null") as {
    snapshot: { epoch: string; hubRevision: number };
  };
  return JSON.stringify({
    type: "snapshot-ack",
    epoch: message.snapshot.epoch,
    hubRevision: message.snapshot.hubRevision,
  });
}

function acknowledgeCurrentSnapshot(
  registry: InstanceType<RegistryModule["HubRegistry"]>,
  connection: FakeConnection,
): void {
  connection.onSend = () => {
    registry.acknowledge(connection, ackJson(connection));
  };
}

function testConfigurationSettersAreIdempotentAndLockedAfterUse(): void {
  const adapter: HubClientDiscoveryAdapter = {
    discoverLiveHubDocuments: () => [],
  };
  const incompatibleAdapter: HubClientDiscoveryAdapter = {
    discoverLiveHubDocuments: () => [],
  };
  const registry = new HubRegistry({
    epoch: "registry-config-lock",
    discovery: adapter,
    reservedRoutes: ["/reserved/one", "/reserved/two"],
  });
  const claim = registry.claimHost("config-lock", "host-config-lock");
  const connection = new FakeConnection("config-lock");
  const expectConfigurationError = (
    action: () => void,
    code: string,
  ): void => {
    try {
      action();
    } catch (error) {
      if (!(error instanceof HubRegistryError)) throw error;
      assertEquals(error.code, code);
      return;
    }
    assert(false, `expected HUB configuration error: ${code}`);
  };

  try {
    registry.registerClient(connection);
    registry.setDiscoveryAdapter(adapter);
    registry.setReservedRoutes([
      "/reserved/two",
      "/reserved/one",
      "/reserved/one",
    ]);
    assert(registry.isUsingDiscoveryAdapterForTests(adapter));
    expectConfigurationError(
      () => registry.setDiscoveryAdapter(incompatibleAdapter),
      "HUB discovery must be configured before use",
    );
    expectConfigurationError(
      () => registry.setReservedRoutes(["/reserved/changed"]),
      "HUB routes must be configured before use",
    );
  } finally {
    registry.removeClient(connection);
    claim.release();
  }

  assertEquals(registry.diagnosticsForTests.clients, 0);
  assertEquals(registry.diagnosticsForTests.claims, 0);
  expectConfigurationError(
    () => registry.setDiscoveryAdapter(incompatibleAdapter),
    "HUB discovery must be configured before use",
  );
  expectConfigurationError(
    () => registry.setReservedRoutes(["/reserved/changed"]),
    "HUB routes must be configured before use",
  );
}

function testStrictDescriptorContract(): void {
  const valid = parseHubPageDescriptor(
    descriptor("experimental-local-sample"),
    "experimental-local-sample",
  );
  assertEquals(valid.route, "/extensions/experimental-local-sample/sample");
  const invalid = [
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      html: "<h1>",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/sample/",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/%2e%2e",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/../sample",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/*",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/sample?query=1",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/sample#fragment",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/features/design",
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      view: { kind: "renderer", source: "x" },
    }),
    '{"localId":"sample","route":"/extensions/experimental-local-sample/sample","titleKey":"a","view":{"kind":"message","titleKey":"a","bodyKey":"b","params":{"__proto__":"x"}}}',
    "{" +
    '"localId":"sample","route":"/extensions/experimental-local-sample/sample","titleKey":"a","view":{"kind":"message","titleKey":"a","bodyKey":"b"},"unknown":true}',
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      view: {
        kind: "message",
        titleKey: "a",
        bodyKey: "b",
        params: { value: "x".repeat(9_000) },
      },
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      titleKey: "x".repeat(129),
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      view: {
        kind: "message",
        titleKey: "a",
        bodyKey: "b",
        params: Object.fromEntries(
          Array.from({ length: 9 }, (_, index) => [`p${index}`, index]),
        ),
      },
    }),
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      view: {
        kind: "message",
        titleKey: "a",
        bodyKey: "b",
        params: { value: 1e100 },
      },
    }),
  ];
  for (const candidate of invalid) {
    let rejected = false;
    try {
      parseHubPageDescriptor(candidate, "experimental-local-sample");
    } catch {
      rejected = true;
    }
    assert(rejected, `invalid descriptor accepted: ${candidate}`);
  }
  assertDescriptorErrorCode(
    "{",
    "experimental-local-sample",
    "invalid hub descriptor JSON",
  );
  assertDescriptorErrorCode(
    descriptor("experimental-local-sample"),
    "Invalid",
    "invalid feature ID",
  );
  assertDescriptorErrorCode(
    " ".repeat(2_049),
    "experimental-local-sample",
    "hub descriptor too large",
  );
  assertDescriptorErrorCode(
    JSON.stringify({
      ...JSON.parse(descriptor("experimental-local-sample")),
      route: "/extensions/experimental-local-sample/not-the-local-id",
    }),
    "experimental-local-sample",
    "non-canonical hub route",
  );
  const registry = new HubRegistry({
    reservedRoutes: ["/extensions/experimental-local-sample/reserved"],
  });
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    const claim = registry.claimHost(
      "experimental-local-sample",
      "host-reserved",
    );
    claims.push(claim);
    const lease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    leases.push(lease);
    let reservedRejected = false;
    try {
      lease.registerDescriptor(descriptor(claim.featureId, "reserved"));
    } catch {
      reservedRejected = true;
    }
    assert(reservedRejected, "reserved built-in routes are rejected");
    lease.dispose();
    claim.release();
    const limitedClaim = registry.claimHost("registry-limit", "host-limit");
    claims.push(limitedClaim);
    const limitedLease = registry.beginGeneration({
      featureId: limitedClaim.featureId,
      hostEpoch: limitedClaim.hostEpoch,
      generation: 1,
    });
    leases.push(limitedLease);
    for (let index = 0; index < 16; index += 1) {
      limitedLease.registerDescriptor(
        descriptor(limitedClaim.featureId, `page-${index}`),
      );
    }
    let limitRejected = false;
    try {
      limitedLease.registerDescriptor(
        descriptor(limitedClaim.featureId, "page-overflow"),
      );
    } catch {
      limitRejected = true;
    }
    assert(limitRejected, "per-owner page count is bounded");
    limitedLease.dispose();
    limitedClaim.release();
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
  }
}

async function testStagingDetachmentAndStaleCleanup(): Promise<void> {
  const registry = new HubRegistry({ epoch: "registry-stage" });
  const featureId = "registry-stage";
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    const claim = registry.claimHost(featureId, "host-stage");
    claims.push(claim);
    const first = registry.beginGeneration({
      featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    leases.push(first);
    first.registerDescriptor(descriptor(featureId));
    let duplicateRejected = false;
    try {
      first.registerDescriptor(descriptor(featureId));
    } catch {
      duplicateRejected = true;
    }
    assert(
      duplicateRejected,
      "duplicate local IDs are rejected within an owner generation",
    );
    assertEquals(registry.getSnapshotForTests().entries.length, 0);
    registry.sealGeneration(first);
    await registry.publishGeneration(first, [], new AbortController().signal);
    const detached = registry.getSnapshotForTests();
    (detached.entries[0].view.params as Record<string, string>).version =
      "mutated";
    (detached.entries as unknown[]).pop();
    assertEquals(
      registry.getSnapshotForTests().entries[0].view.params?.version,
      "hub-v1",
    );
    const second = registry.beginGeneration({
      featureId,
      hostEpoch: claim.hostEpoch,
      generation: 2,
    });
    leases.push(second);
    second.registerDescriptor(descriptor(featureId));
    registry.sealGeneration(second);
    await registry.publishGeneration(second, [], new AbortController().signal);
    first.dispose();
    assertEquals(registry.getSnapshotForTests().entries[0].ownerGeneration, 2);
    first.dispose();
    second.dispose();
    assertEquals(registry.getSnapshotForTests().entries.length, 0);
    claim.release();
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
  }
}

async function testMonotonicGenerationsAndHostEpochIdentity(): Promise<void> {
  const registry = new HubRegistry({ epoch: "registry-generations" });
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    const claim = registry.claimHost("registry-generations", "host-first");
    claims.push(claim);
    const first = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    leases.push(first);
    first.registerDescriptor(descriptor(claim.featureId));
    registry.sealGeneration(first);

    let duplicateRejected = false;
    try {
      registry.beginGeneration({ ...first.owner });
    } catch {
      duplicateRejected = true;
    }
    assert(duplicateRejected, "duplicate generation leases are rejected");

    const second = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 2,
    });
    leases.push(second);
    second.registerDescriptor(descriptor(claim.featureId));
    registry.sealGeneration(second);

    let staleRejected = false;
    try {
      await registry.publishGeneration(first, [], new AbortController().signal);
    } catch (error) {
      staleRejected = String(error).includes("stale HUB generation");
    }
    assert(staleRejected, "an older staged generation cannot publish");
    assertEquals(registry.getSnapshotForTests().entries.length, 0);

    await registry.publishGeneration(second, [], new AbortController().signal);
    first.dispose();
    first.dispose();
    let snapshot = registry.getSnapshotForTests();
    assertEquals(snapshot.entries[0].ownerHostEpoch, "host-first");
    assertEquals(snapshot.entries[0].ownerGeneration, 2);
    second.dispose();
    second.dispose();
    assertEquals(registry.getSnapshotForTests().entries.length, 0);
    claim.release();

    const reopenedClaim = registry.claimHost(
      "registry-generations",
      "host-reopened",
    );
    claims.push(reopenedClaim);
    const reopened = registry.beginGeneration({
      featureId: reopenedClaim.featureId,
      hostEpoch: reopenedClaim.hostEpoch,
      generation: 1,
    });
    leases.push(reopened);
    reopened.registerDescriptor(descriptor(reopenedClaim.featureId));
    registry.sealGeneration(reopened);
    await registry.publishGeneration(
      reopened,
      [],
      new AbortController().signal,
    );
    snapshot = registry.getSnapshotForTests();
    assertEquals(snapshot.entries[0].ownerHostEpoch, "host-reopened");
    assertEquals(snapshot.entries[0].ownerGeneration, 1);
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
  }
}

async function testParentVisibleW0AndNeverConnectedTimeout(): Promise<void> {
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    const connection = new FakeConnection("never-connect");
    const discovery = new FakeDiscovery([new FakeDocument(connection)]);
    const registry = new HubRegistry({ epoch: "registry-w0", discovery });
    const w0 = registry.captureW0(["window-1"]);
    const newcomer = new FakeConnection("late-client", "window-1");
    discovery.documents.push(new FakeDocument(newcomer));
    assertEquals(w0.length, 1);
    assertEquals(discovery.queriedWindows.join(","), "window-1");
    const claim = registry.claimHost("registry-w0", "host-w0");
    claims.push(claim);
    const lease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    leases.push(lease);
    lease.registerDescriptor(descriptor(claim.featureId));
    registry.sealGeneration(lease);
    let timedOut = false;
    try {
      await registry.publishGeneration(
        lease,
        w0,
        new AbortController().signal,
        20,
      );
    } catch (error) {
      timedOut =
        errorMessage(error) === "publication acknowledgement timed out";
    }
    assert(
      timedOut,
      "a live parent-visible document that never connects must block readiness",
    );
    assertEquals(
      connection.isClosed(),
      false,
      "live disconnected client remains required until timeout",
    );
    assertEquals(connection.messages.length, 2);
    assertEquals(
      newcomer.messages.length,
      0,
      "new clients do not join captured W0",
    );
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 0);
    lease.dispose();
    claim.release();

    const outside = new FakeDocument(new FakeConnection("outside", "window-2"));
    const outsideRegistry = new HubRegistry({
      discovery: { discoverLiveHubDocuments: () => [outside] },
    });
    let outsideRejected = false;
    try {
      outsideRegistry.captureW0(["window-1"]);
    } catch {
      outsideRejected = true;
    }
    assert(
      outsideRejected,
      "documents outside the captured window set are rejected",
    );

    const missingClientRegistry = new HubRegistry({
      discovery: {
        discoverLiveHubDocuments: () => [{
          documentId: "missing-client",
          windowId: "window-1",
          isLive: () => true,
          ensureParentClient: () => null,
          onParentClientAvailable: () => () => {},
          onClosed: () => () => {},
        }],
      },
    });
    const missingClientW0 = missingClientRegistry.captureW0(["window-1"]);
    const missingClientClaim = missingClientRegistry.claimHost(
      "registry-missing",
      "host-missing",
    );
    claims.push(missingClientClaim);
    const missingClientLease = missingClientRegistry.beginGeneration({
      featureId: missingClientClaim.featureId,
      hostEpoch: missingClientClaim.hostEpoch,
      generation: 1,
    });
    leases.push(missingClientLease);
    missingClientLease.registerDescriptor(
      descriptor(missingClientClaim.featureId),
    );
    missingClientRegistry.sealGeneration(missingClientLease);
    let missingClientTimedOut = false;
    try {
      await missingClientRegistry.publishGeneration(
        missingClientLease,
        missingClientW0,
        new AbortController().signal,
        20,
      );
    } catch (error) {
      missingClientTimedOut = errorMessage(error) ===
        "publication acknowledgement timed out";
    }
    assert(
      missingClientTimedOut,
      "a live document without a parent client blocks readiness",
    );
    missingClientLease.dispose();
    missingClientClaim.release();
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
  }
}

async function testBroadcastsAllClientsButGatesOnlyCapturedW0(): Promise<void> {
  const w0Connection = new FakeConnection("broadcast-w0", "window-1");
  const w0Document = new FakeDocument(w0Connection);
  const registry = new HubRegistry({
    epoch: "registry-broadcast",
    discovery: new FakeDiscovery([w0Document]),
  });
  acknowledgeCurrentSnapshot(registry, w0Connection);
  const w0 = registry.captureW0(["window-1"]);
  const lateConnection = new FakeConnection("broadcast-late", "window-1");
  const outsideConnection = new FakeConnection("broadcast-outside", "window-2");
  registry.registerClient(lateConnection);
  registry.registerClient(outsideConnection);
  assertEquals(w0.length, 1);

  let claim: HubHostClaim | null = null;
  let lease: HubGenerationLease | null = null;
  try {
    claim = registry.claimHost("registry-broadcast", "host-broadcast");
    lease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    lease.registerDescriptor(descriptor(claim.featureId));
    registry.sealGeneration(lease);
    const snapshot = await registry.publishGeneration(
      lease,
      w0,
      new AbortController().signal,
      20,
    );

    assertEquals(snapshot.hubRevision, 1);
    for (
      const connection of [w0Connection, lateConnection, outsideConnection]
    ) {
      const delivered = JSON.parse(connection.messages.at(-1) ?? "null") as {
        snapshot: { hubRevision: number; entries: readonly unknown[] };
      };
      assertEquals(delivered.snapshot.hubRevision, 1);
      assertEquals(delivered.snapshot.entries.length, 1);
    }
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "non-W0 clients do not participate in the publication gate",
    );
  } finally {
    lease?.dispose();
    claim?.release();
    w0Connection.close();
    lateConnection.close();
    outsideConnection.close();
  }
  assertEquals(registry.diagnosticsForTests.clients, 0);
}

async function testStaleAcknowledgementsAndClosedClient(): Promise<void> {
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    const connection = new FakeConnection("ack");
    const registry = new HubRegistry({
      epoch: "registry-ack",
      discovery: new FakeDiscovery([new FakeDocument(connection)]),
    });
    const w0 = registry.captureW0(["window-1"]);
    const claim = registry.claimHost("registry-ack", "host-ack");
    claims.push(claim);
    const lease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    leases.push(lease);
    lease.registerDescriptor(descriptor(claim.featureId));
    registry.sealGeneration(lease);
    connection.onSend = () => {
      assert(
        !registry.acknowledge(
          connection,
          JSON.stringify({
            type: "snapshot-ack",
            epoch: "wrong",
            hubRevision: 1,
          }),
        ),
      );
      assert(
        !registry.acknowledge(
          connection,
          JSON.stringify({
            type: "snapshot-ack",
            epoch: "registry-ack",
            hubRevision: 999,
          }),
        ),
      );
      assert(
        !registry.acknowledge(
          new FakeConnection("foreign"),
          ackJson(connection),
        ),
      );
      assert(registry.acknowledge(connection, ackJson(connection)));
    };
    await registry.publishGeneration(
      lease,
      w0,
      new AbortController().signal,
      100,
    );
    lease.dispose();
    claim.release();

    const closing = new FakeConnection("closing");
    const closingDocument = new FakeDocument(closing);
    const closeRegistry = new HubRegistry({
      epoch: "registry-close",
      discovery: new FakeDiscovery([closingDocument]),
    });
    const closeW0 = closeRegistry.captureW0(["window-1"]);
    const closeClaim = closeRegistry.claimHost("registry-close", "host-close");
    claims.push(closeClaim);
    const closeLease = closeRegistry.beginGeneration({
      featureId: closeClaim.featureId,
      hostEpoch: closeClaim.hostEpoch,
      generation: 1,
    });
    leases.push(closeLease);
    closeLease.registerDescriptor(descriptor(closeClaim.featureId));
    closeRegistry.sealGeneration(closeLease);
    closing.onSend = () => {
      closingDocument.closeDocument();
      closing.close();
    };
    await closeRegistry.publishGeneration(
      closeLease,
      closeW0,
      new AbortController().signal,
      100,
    );
    closeLease.dispose();
    closeClaim.release();

    const alreadyClosedConnection = new FakeConnection("already-closed");
    const alreadyClosedDocument = new FakeDocument(alreadyClosedConnection);
    const alreadyClosedRegistry = new HubRegistry({
      epoch: "registry-already-closed",
      discovery: new FakeDiscovery([alreadyClosedDocument]),
    });
    const alreadyClosedW0 = alreadyClosedRegistry.captureW0(["window-1"]);
    alreadyClosedDocument.closeDocument();
    const alreadyClosedClaim = alreadyClosedRegistry.claimHost(
      "registry-already-closed",
      "host-already-closed",
    );
    claims.push(alreadyClosedClaim);
    const alreadyClosedLease = alreadyClosedRegistry.beginGeneration({
      featureId: alreadyClosedClaim.featureId,
      hostEpoch: alreadyClosedClaim.hostEpoch,
      generation: 1,
    });
    leases.push(alreadyClosedLease);
    alreadyClosedLease.registerDescriptor(
      descriptor(alreadyClosedClaim.featureId),
    );
    alreadyClosedRegistry.sealGeneration(alreadyClosedLease);
    await alreadyClosedRegistry.publishGeneration(
      alreadyClosedLease,
      alreadyClosedW0,
      new AbortController().signal,
      100,
    );
    assertEquals(
      alreadyClosedRegistry.diagnosticsForTests.clients,
      0,
      "a closed W0 document releases its parent connection",
    );
    alreadyClosedLease.dispose();
    alreadyClosedClaim.release();
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
  }
}

async function testDisconnectedLiveDocumentWaitsForResync(): Promise<void> {
  let claim: ReturnType<InstanceType<typeof HubRegistry>["claimHost"]> | null =
    null;
  let lease:
    | ReturnType<InstanceType<typeof HubRegistry>["beginGeneration"]>
    | null = null;
  try {
    const firstConnection = new FakeConnection("disconnect");
    const document = new FakeDocument(firstConnection);
    const registry = new HubRegistry({
      epoch: "registry-resync",
      discovery: new FakeDiscovery([document]),
    });
    const w0 = registry.captureW0(["window-1"]);
    claim = registry.claimHost("registry-resync", "host-resync");
    lease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    lease.registerDescriptor(descriptor(claim.featureId));
    registry.sealGeneration(lease);
    firstConnection.onSend = () => firstConnection.close();
    const publication = registry.publishGeneration(
      lease,
      w0,
      new AbortController().signal,
      200,
    );
    assertEquals(firstConnection.isClosed(), true);
    assertEquals(document.isLive(), true);
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);
    const resync = new FakeConnection("resync");
    resync.onSend = () => registry.acknowledge(resync, ackJson(resync));
    document.replaceConnection(resync);
    await publication;
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 0);
  } finally {
    lease?.dispose();
    claim?.release();
  }
}

async function testLatestFullSnapshotSatisfiesMultipleFeatures(): Promise<
  void
> {
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    const connection = new FakeConnection("multi");
    const discovery = new FakeDiscovery([new FakeDocument(connection)]);
    const registry = new HubRegistry({ epoch: "registry-multi", discovery });
    const w0 = registry.captureW0(["window-1"]);
    const claimA = registry.claimHost("feature-a", "host-a");
    claims.push(claimA);
    const claimB = registry.claimHost("feature-b", "host-b");
    claims.push(claimB);
    const leaseA = registry.beginGeneration({
      featureId: "feature-a",
      hostEpoch: "host-a",
      generation: 1,
    });
    leases.push(leaseA);
    const leaseB = registry.beginGeneration({
      featureId: "feature-b",
      hostEpoch: "host-b",
      generation: 1,
    });
    leases.push(leaseB);
    leaseA.registerDescriptor(descriptor("feature-a"));
    leaseB.registerDescriptor(descriptor("feature-b"));
    registry.sealGeneration(leaseA);
    registry.sealGeneration(leaseB);
    const signal = new AbortController().signal;
    const first = registry.publishGeneration(leaseA, w0, signal, 200);
    const second = registry.publishGeneration(leaseB, w0, signal, 200);
    assertEquals(
      JSON.parse(connection.messages.at(-1) ?? "null").snapshot.entries.length,
      2,
    );
    assert(registry.acknowledge(connection, ackJson(connection)));
    await Promise.all([first, second]);
    leaseA.dispose();
    leaseB.dispose();
    claimA.release();
    claimB.release();
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
  }
}

async function testRemovalWaitsRequireAbsenceAndCurrentClaim(): Promise<void> {
  const connection = new FakeConnection("remove-wait", "window-1");
  const discovery = new FakeDiscovery([new FakeDocument(connection)]);
  const registry = new HubRegistry({
    epoch: "registry-remove-wait",
    discovery,
  });
  const w0 = registry.captureW0(["window-1"]);
  const claims: Array<{ release(): void }> = [];
  const leases: Array<{ dispose(): void }> = [];
  try {
    let targetClaim = registry.claimHost("remove-target", "host-first");
    claims.push(targetClaim);
    let target = registry.beginGeneration({
      featureId: targetClaim.featureId,
      hostEpoch: targetClaim.hostEpoch,
      generation: 1,
    });
    leases.push(target);
    target.registerDescriptor(descriptor(targetClaim.featureId));
    registry.sealGeneration(target);
    acknowledgeCurrentSnapshot(registry, connection);
    await registry.publishGeneration(target, w0, new AbortController().signal);

    connection.onSend = null;
    const removal = registry.publishRemoval(
      targetClaim,
      w0,
      new AbortController().signal,
      1_000,
    );
    const removalResult = rejectionOf(removal);
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);

    const replacement = registry.beginGeneration({
      featureId: targetClaim.featureId,
      hostEpoch: targetClaim.hostEpoch,
      generation: 2,
    });
    leases.push(replacement);
    replacement.registerDescriptor(descriptor(targetClaim.featureId));
    registry.sealGeneration(replacement);
    acknowledgeCurrentSnapshot(registry, connection);
    await registry.publishGeneration(
      replacement,
      w0,
      new AbortController().signal,
    );
    assertEquals(errorMessage(await removalResult), "HUB removal superseded");
    assertEquals(registry.getSnapshotForTests().entries[0].ownerGeneration, 2);
    target.dispose();
    replacement.dispose();
    targetClaim.release();

    targetClaim = registry.claimHost("remove-target", "host-reopened");
    claims.push(targetClaim);
    target = registry.beginGeneration({
      featureId: targetClaim.featureId,
      hostEpoch: targetClaim.hostEpoch,
      generation: 1,
    });
    leases.push(target);
    target.registerDescriptor(descriptor(targetClaim.featureId));
    registry.sealGeneration(target);
    acknowledgeCurrentSnapshot(registry, connection);
    await registry.publishGeneration(target, w0, new AbortController().signal);

    connection.onSend = null;
    const unrelatedRemoval = registry.publishRemoval(
      targetClaim,
      w0,
      new AbortController().signal,
      1_000,
    );
    const unrelatedResult = rejectionOf(unrelatedRemoval);
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);
    const otherClaim = registry.claimHost("remove-other", "host-other");
    claims.push(otherClaim);
    const other = registry.beginGeneration({
      featureId: otherClaim.featureId,
      hostEpoch: otherClaim.hostEpoch,
      generation: 1,
    });
    leases.push(other);
    other.registerDescriptor(descriptor(otherClaim.featureId));
    registry.sealGeneration(other);
    acknowledgeCurrentSnapshot(registry, connection);
    await registry.publishGeneration(other, w0, new AbortController().signal);
    assertEquals(await unrelatedResult, null);
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "a newer unrelated snapshot releases the removal waiter",
    );
    const snapshot = registry.getSnapshotForTests();
    assertEquals(
      snapshot.entries.some((entry) =>
        entry.featureId === targetClaim.featureId
      ),
      false,
    );
    targetClaim.release();
    assertEquals(
      snapshot.entries.some((entry) =>
        entry.featureId === otherClaim.featureId
      ),
      true,
    );

    connection.onSend = null;
    const releaseWait = registry.publishRemoval(
      otherClaim,
      w0,
      new AbortController().signal,
      1_000,
    );
    const releaseResult = rejectionOf(releaseWait);
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);
    otherClaim.release();
    assertEquals(errorMessage(await releaseResult), "hub host claim revoked");
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "claim release removes its pending acknowledgement waiter",
    );
    assertEquals(
      registry.diagnosticsForTests.pendingTimers,
      0,
      "claim release clears its publication deadline",
    );
    assertEquals(registry.diagnosticsForTests.claims, 0, "all claims released");
  } finally {
    for (const lease of leases) lease.dispose();
    for (const claim of claims) claim.release();
    connection.close();
  }
}

async function testPublicationAbortAndDeadlineValidation(): Promise<void> {
  const connection = new FakeConnection("publication-abort", "window-1");
  const discovery = new FakeDiscovery([new FakeDocument(connection)]);
  const registry = new HubRegistry({
    epoch: "registry-publication-abort",
    discovery,
  });
  const w0 = registry.captureW0(["window-1"]);
  const claim = registry.claimHost("publication-abort", "host-abort");
  const lease = registry.beginGeneration({
    featureId: claim.featureId,
    hostEpoch: claim.hostEpoch,
    generation: 1,
  });
  lease.registerDescriptor(descriptor(claim.featureId));
  registry.sealGeneration(lease);
  const reason = new Error("publication cancelled by owner");
  const alreadyAborted = new AbortController();
  alreadyAborted.abort(reason);
  try {
    for (const documents of [[], w0]) {
      assertEquals(
        errorMessage(
          await rejectionOf(
            registry.publishGeneration(lease, documents, alreadyAborted.signal),
          ),
        ),
        reason.message,
      );
      assertEquals(
        errorMessage(
          await rejectionOf(
            registry.publishRemoval(claim, documents, alreadyAborted.signal),
          ),
        ),
        reason.message,
      );
    }
    const invalidDeadline = await rejectionOf(
      registry.publishRemoval(
        claim,
        [],
        new AbortController().signal,
        0,
      ),
    );
    assertEquals(
      errorMessage(invalidDeadline),
      "invalid HUB publication deadline",
    );
    assertEquals(registry.getSnapshotForTests().hubRevision, 0);
    assertEquals(registry.getSnapshotForTests().entries.length, 0);

    const inFlightController = new AbortController();
    const inFlightReason = new Error("publication aborted in flight");
    const publication = registry.publishGeneration(
      lease,
      w0,
      inFlightController.signal,
      5_000,
    );
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);
    assertEquals(registry.diagnosticsForTests.pendingTimers, 1);
    inFlightController.abort(inFlightReason);
    assertEquals(
      errorMessage(await rejectionOf(publication)),
      inFlightReason.message,
    );
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 0);
    assertEquals(registry.diagnosticsForTests.pendingTimers, 0);
  } finally {
    lease.dispose();
    claim.release();
    connection.close();
  }
}

async function testAbortAfterAckBeforeWaitContinuationIsRejected(): Promise<
  void
> {
  const connection = new FakeConnection("final-abort", "window-1");
  const discovery = new FakeDiscovery([new FakeDocument(connection)]);
  const registry = new HubRegistry({
    epoch: "registry-final-abort",
    discovery,
  });
  const w0 = registry.captureW0(["window-1"]);
  const claim = registry.claimHost("final-abort", "host-final-abort");
  const lease = registry.beginGeneration({
    featureId: claim.featureId,
    hostEpoch: claim.hostEpoch,
    generation: 1,
  });
  lease.registerDescriptor(descriptor(claim.featureId));
  registry.sealGeneration(lease);
  const controller = new AbortController();
  const reason = new Error("abort after acknowledgement");
  let scheduled = false;
  connection.onSend = (json) => {
    if (scheduled) return;
    scheduled = true;
    const message = JSON.parse(json) as {
      snapshot: { epoch: string; hubRevision: number };
    };
    setTimeout(() => {
      registry.acknowledge(
        connection,
        JSON.stringify({
          type: "snapshot-ack",
          epoch: message.snapshot.epoch,
          hubRevision: message.snapshot.hubRevision,
        }),
      );
      queueMicrotask(() => controller.abort(reason));
    }, 0);
  };
  try {
    const publication = registry.publishGeneration(
      lease,
      w0,
      controller.signal,
      1_000,
    );
    assertEquals(
      errorMessage(await rejectionOf(publication)),
      reason.message,
    );
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 0);
    assertEquals(registry.diagnosticsForTests.pendingTimers, 0);
  } finally {
    lease.dispose();
    claim.release();
    connection.close();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name:
        "HUB discovery and reserved-route configuration is idempotent and locked after use",
      fn: testConfigurationSettersAreIdempotentAndLockedAfterUse,
    },
    {
      name: "HUB descriptor JSON rejects unsafe and non-canonical data",
      fn: testStrictDescriptorContract,
    },
    {
      name:
        "HUB registration stages, detaches snapshots, and binds cleanup to generation",
      fn: testStagingDetachmentAndStaleCleanup,
    },
    {
      name:
        "HUB W0 comes from live documents and never-connected documents time out",
      fn: testParentVisibleW0AndNeverConnectedTimeout,
    },
    {
      name:
        "HUB broadcasts revisions to all clients but gates only captured W0",
      fn: testBroadcastsAllClientsButGatesOnlyCapturedW0,
    },
    {
      name:
        "HUB generations are monotonic and snapshot keys include host epoch",
      fn: testMonotonicGenerationsAndHostEpochIdentity,
    },
    {
      name:
        "HUB acknowledgements reject stale epochs and revisions and release closed clients",
      fn: testStaleAcknowledgementsAndClosedClient,
    },
    {
      name: "disconnected but live HUB documents remain required until resync",
      fn: testDisconnectedLiveDocumentWaitsForResync,
    },
    {
      name:
        "a latest complete HUB snapshot acknowledges multiple feature publications",
      fn: testLatestFullSnapshotSatisfiesMultipleFeatures,
    },
    {
      name:
        "HUB removal waits require absence and remain bound to the current claim",
      fn: testRemovalWaitsRequireAbsenceAndCurrentClaim,
    },
    {
      name:
        "HUB publication validates aborts and deadlines before effects and cleans up waits",
      fn: testPublicationAbortAndDeadlineValidation,
    },
    {
      name:
        "HUB abort after acknowledgement but before wait continuation is rejected",
      fn: testAbortAfterAckBeforeWaitContinuationIsRejected,
    },
  ];
  await runTests("HubRegistry.test.mts", tests);
}
