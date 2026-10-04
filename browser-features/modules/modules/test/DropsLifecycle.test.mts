// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate";
import {
  type DepRef,
  type DropEntry,
  type DropInspection,
  type DropManifest,
  inspectDrop,
  installDrop,
  listDrops,
  listRegistries,
  removeDrop,
  verifyDrop,
} from "../Drops.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const encoder = new TextEncoder();

interface Fixture {
  manifest: DropManifest;
  bytes: Map<string, Uint8Array>;
}

function newUuid(): string {
  return Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
}

function directory(uuid: string): string {
  return PathUtils.join(PathUtils.profileDir, "noraneko-drops", uuid);
}

function actorName(uuid: string, suffix = ""): string {
  return "DropsTest" + uuid.replaceAll("-", "") + suffix;
}

function alias(uuid: string, version: string, dependency = false): string {
  return ("noraneko-" + (dependency ? "dep-" : "drop-") + uuid + "-" + version)
    .replace(/[^a-z0-9]/gi, "-").toLowerCase();
}

function resources(): nsIResProtocolHandler {
  return Services.io.getProtocolHandler("resource")!.QueryInterface!(
    Ci.nsIResProtocolHandler,
  );
}

async function digest(bytes: Uint8Array<ArrayBuffer>): Promise<string> {
  const result = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
  return [...result].map((value) => value.toString(16).padStart(2, "0")).join(
    "",
  );
}

async function fixture(
  uuid: string,
  version = "1.0.0",
  options: {
    dependency?: DepRef;
    invalidActor?: boolean;
    missingActor?: boolean;
    marker?: string;
    suffix?: string;
  } = {},
): Promise<Fixture> {
  const suffix = options.suffix ?? "";
  const name = actorName(uuid, suffix);
  const id = uuid + suffix + "@floorp-drops-test.invalid";
  const archive: Record<string, Uint8Array> = {
    "manifest.json": encoder.encode(JSON.stringify({
      manifest_version: 2,
      name: "Floorp Drops lifecycle fixture",
      version,
      browser_specific_settings: { gecko: { id } },
    })),
    "parent.sys.mjs": encoder.encode(
      "export class " + name + "Parent extends JSWindowActorParent {}",
    ),
    "child.sys.mjs": encoder.encode(
      "export class " + name + "Child extends JSWindowActorChild {}",
    ),
    "content.js": encoder.encode(options.marker ?? "/* approved fixture */"),
  };
  if (!options.missingActor) {
    archive["actor.json"] = encoder.encode(JSON.stringify({
      name,
      id,
      version,
      event: "DOMContentLoaded",
      matches: options.invalidActor
        ? ["not-a-match-pattern"]
        : ["https://floorp-drops-test.invalid/*"],
      methods: [],
      includeParent: false,
    }));
  }
  const bytes = zipSync(archive);
  const file = "fixture" + suffix + ".xpi";
  const entry: DropEntry = {
    id,
    name: "fixture",
    version,
    file,
    size: bytes.length,
    sha256: await digest(new Uint8Array(bytes)),
  };
  return {
    manifest: {
      uuid,
      name: "fixture",
      entries: [entry],
      deps: options.dependency ? [options.dependency] : [],
    },
    bytes: new Map([[file, bytes]]),
  };
}

function fixtureFetch(fixtures: Fixture[]): typeof fetch {
  return (input) => {
    const url = typeof input === "string"
      ? input
      : input instanceof URL
      ? input.href
      : input.url;
    if (url.endsWith(".sigstore.json")) {
      return Promise.resolve(new Response("", { status: 404 }));
    }
    for (const f of fixtures) {
      const base = "/" + f.manifest.uuid;
      const versioned = base + "/v/" + f.manifest.entries[0].version;
      if (
        url.endsWith(base + "/manifest.json") ||
        url.endsWith(versioned + "/manifest.json")
      ) {
        return Promise.resolve(Response.json(f.manifest));
      }
      for (const [file, bytes] of f.bytes) {
        if (
          url.endsWith(base + "/" + file) ||
          url.endsWith(versioned + "/" + file)
        ) {
          return Promise.resolve(new Response(new Uint8Array(bytes)));
        }
      }
    }
    throw new Error("Unexpected fixture fetch: " + url);
  };
}

async function inspect(
  f: Fixture,
  libraries: Fixture[] = [],
): Promise<DropInspection> {
  return await inspectDrop(
    f.manifest.uuid,
    listRegistries()[0].name,
    fixtureFetch([f, ...libraries]),
  );
}

async function rejects(fn: () => Promise<unknown>): Promise<void> {
  let rejected = false;
  try {
    await fn();
  } catch {
    rejected = true;
  }
  assert(rejected, "operation should reject");
}

async function cleanup(uuids: string[]): Promise<void> {
  for (const uuid of uuids) {
    await removeDrop(uuid);
    await IOUtils.remove(directory(uuid), {
      recursive: true,
      ignoreAbsent: true,
    });
  }
}

async function testInspectionPreservesApprovedBytes(): Promise<void> {
  const uuid = newUuid();
  try {
    const original = await fixture(uuid);
    const originalInspection = await inspect(original);
    await installDrop(originalInspection);
    const path = PathUtils.join(directory(uuid), "1.0.0", "fixture.xpi");
    const approvedHash = await IOUtils.computeHexDigest(path, "sha256");
    const changed = await fixture(uuid, "1.0.0", { marker: "/* changed */" });
    const seen = await inspect(changed);
    assertEquals(
      (await verifyDrop(seen)).ok,
      true,
      "new candidate should verify",
    );
    assertEquals(
      await IOUtils.computeHexDigest(path, "sha256"),
      approvedHash,
      "inspection must preserve approved bytes",
    );
    await rejects(() => installDrop(seen));
    assertEquals(
      await IOUtils.computeHexDigest(path, "sha256"),
      approvedHash,
      "version reuse must not replace approved bytes",
    );
    changed.manifest.entries[0].sha256 = original.manifest.entries[0].sha256;
    await rejects(() => inspect(changed));
    assertEquals(
      await IOUtils.computeHexDigest(path, "sha256"),
      approvedHash,
      "failed inspection must not delete approved bytes",
    );
    assert(listDrops()[uuid], "approved installation must remain listed");
    assertEquals(
      (await verifyDrop(originalInspection)).ok,
      true,
      "a failed inspection must preserve an already verified candidate",
    );
  } finally {
    await cleanup([uuid]);
  }
}

async function testSharedDependencySurvivesRemoval(): Promise<void> {
  for (const removeLast of [true, false]) {
    const first = newUuid(), second = newUuid(), libraryUuid = newUuid();
    const library = await fixture(libraryUuid);
    const dependency: DepRef = {
      name: "shared",
      uuid: libraryUuid,
      version: "1.0.0",
      lib: true,
      wasm: false,
    };
    try {
      await installDrop(
        await inspect(await fixture(first, "1.0.0", { dependency }), [library]),
      );
      await installDrop(
        await inspect(await fixture(second, "1.0.0", { dependency }), [
          library,
        ]),
      );
      const removed = removeLast ? second : first;
      const remaining = removeLast ? first : second;
      await removeDrop(removed);
      const depAlias = alias(libraryUuid, "1.0.0", true);
      const uri = resources().getSubstitution(depAlias).QueryInterface!(
        Ci.nsIJARURI,
      );
      const file = uri.JARFile.QueryInterface!(Ci.nsIFileURL).file;
      assert(
        file.exists(),
        "shared dependency alias must target an existing file",
      );
      assert(
        file.path.includes(remaining),
        "shared dependency must use the surviving consumer's copy",
      );
      await removeDrop(remaining);
      assertEquals(
        resources().hasSubstitution(depAlias),
        false,
        "final removal clears dependency alias",
      );
    } finally {
      await cleanup([first, second]);
    }
  }
}

async function testActorFailureRollsBackAddon(): Promise<void> {
  for (const missingActor of [false, true]) {
    const uuid = newUuid();
    try {
      const seen = await inspect(
        await fixture(uuid, "1.0.0", {
          invalidActor: !missingActor,
          missingActor,
        }),
      );
      await rejects(() => installDrop(seen));
      assertEquals(
        await AddonManager.getAddonByID(seen.manifest.entries[0].id),
        null,
        "failed actor setup must not leave a temporary add-on",
      );
      assertEquals(
        listDrops()[uuid],
        undefined,
        "failed install must not be listed",
      );
      assertEquals(
        resources().hasSubstitution(alias(uuid, "1.0.0")),
        false,
        "failed install must not leave a resource alias",
      );
      const { isRegistered } = ChromeUtils.importESModule(
        "resource://noraneko/modules/NoraActors.sys.mjs",
      );
      assertEquals(
        isRegistered(actorName(uuid)),
        false,
        "failed actor must be unregistered",
      );
    } finally {
      await cleanup([uuid]);
    }
  }
}

async function testLaterEntryFailureRestoresPreviousVersion(): Promise<void> {
  const uuid = newUuid();
  try {
    await installDrop(await inspect(await fixture(uuid)));
    const candidate = await fixture(uuid, "2.0.0");
    const broken = await fixture(uuid, "2.0.1", {
      invalidActor: true,
      suffix: "Broken",
    });
    candidate.manifest.entries.push(...broken.manifest.entries);
    for (const [file, bytes] of broken.bytes) candidate.bytes.set(file, bytes);
    await rejects(async () => installDrop(await inspect(candidate)));
    const oldAddon = await AddonManager.getAddonByID(
      uuid + "@floorp-drops-test.invalid",
    );
    assertEquals(oldAddon?.version, "1.0.0", "prior add-on must be restored");
    assertEquals(
      listDrops()[uuid].versions[0],
      "1.0.0",
      "prior installed row must survive",
    );
    assertEquals(
      await AddonManager.getAddonByID(
        uuid + "Broken@floorp-drops-test.invalid",
      ),
      null,
      "failing later add-on must be removed",
    );
    assertEquals(
      resources().hasSubstitution(alias(uuid, "2.0.0")),
      false,
      "earlier candidate alias must be removed",
    );
    assertEquals(
      resources().hasSubstitution(alias(uuid, "2.0.1")),
      false,
      "failing candidate alias must be removed",
    );
    assertEquals(
      resources().hasSubstitution(alias(uuid, "1.0.0")),
      true,
      "prior alias must be restored",
    );
  } finally {
    await cleanup([uuid]);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "inspection preserves approved bytes even on failure",
      fn: testInspectionPreservesApprovedBytes,
    },
    {
      name: "shared dependencies survive either consumer removal order",
      fn: testSharedDependencySurvivesRemoval,
    },
    {
      name: "actor setup failures leave no unmanaged add-ons",
      fn: testActorFailureRollsBackAddon,
    },
    {
      name: "later entry failure restores previous installed version",
      fn: testLaterEntryFailureRestoresPreviousVersion,
    },
  ];
  await runTests("DropsLifecycle.test.mts", tests);
}
