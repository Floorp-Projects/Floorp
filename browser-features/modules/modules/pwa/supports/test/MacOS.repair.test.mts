// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getMacAppBundle, MacOSSupport, type MacAppOptions } from "../MacOS.sys.mts";
import { MockMacOSSupport } from "./MockMacOSSupport.ts";
import type { Manifest } from "../../type.ts";
import type { AppRegistryState } from "#libs/pwa/appRegistryTypes.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const app: Manifest = {
  id: "{12345678-1234-5678-1234-567812345678}",
  name: "R",
  start_url: "https://example.com/",
  icon: "",
};

class RepairSupport extends MockMacOSSupport {
  compiled = 0;
  signed = 0;
  registered = 0;
  protected override registerLegacyApp(): Promise<AppRegistryState> {
    this.registered++;
    return Promise.resolve({} as AppRegistryState);
  }
  protected override async compileApplet(path: string): Promise<void> {
    this.compiled++;
    await super.compileApplet(path);
  }
  protected override async signApplet(path: string, bundleId: string): Promise<void> {
    this.signed++;
    await super.signApplet(path, bundleId);
  }
  protected override createIcon(): Promise<Uint8Array> {
    return Promise.resolve(new Uint8Array([1, 2, 3]));
  }
}

async function runRepair(startup: boolean): Promise<void> {
  const root = PathUtils.join(
    PathUtils.tempDir,
    // Keep nested staged paths below legacy Windows path limits.
    `r-${crypto.randomUUID().slice(0, 12)}`,
  );
  const options: MacAppOptions = {
    applicationsDir: PathUtils.join(root, "a"),
    profileDir: PathUtils.join(root, "p"),
    executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
  };
  const support = new RepairSupport(options);
  const bundle = await getMacAppBundle(app, options);
  const contents = PathUtils.join(bundle.path, "Contents");
  const launcher = PathUtils.join(contents, "Resources", "launcher.sh");
  const executable = PathUtils.join(contents, "MacOS", "applet");
  const plist = PathUtils.join(contents, "Info.plist");
  const marker = PathUtils.join(contents, "floorp.json");
  let storeWrites = 0;
  const store = {
    getCurrentSsbData: () => Promise.resolve({ [app.id]: app }),
    saveSsbData: () => { storeWrites++; return Promise.resolve(); },
    removeSsbData: () => { storeWrites++; return Promise.resolve(); },
  };
  const repair = () =>
    startup
      ? support.repairExistingLaunchers(store)
      : support.repair(app, store);
  try {
    await IOUtils.makeDirectory(PathUtils.join(options.profileDir, "ssb"), {
      createAncestors: true,
      ignoreExisting: true,
    });
    await IOUtils.writeJSON(
      PathUtils.join(options.profileDir, "ssb", "ssb.json"),
      {},
    );
    await support.install(app);
    const originalMarker = await IOUtils.readUTF8(marker);
    assertEquals(support.signed, 1, "initial bundle was signed");
    await IOUtils.writeUTF8(launcher, "#!/bin/sh\nexit 1\n");
    await repair();
    assertEquals(
      await IOUtils.readUTF8(launcher),
      bundle.launcher,
      "damaged launcher is restored",
    );
    assertEquals(
      support.signed,
      2,
      "launcher repair recompiles and signs replacement",
    );
    await IOUtils.writeUTF8(plist, "damaged plist");
    await repair();
    assertEquals(
      await IOUtils.readUTF8(plist),
      bundle.plist,
      "damaged plist is restored",
    );
    assertEquals(support.signed, 3, "plist repair signs replacement");
    if (Services.appinfo.OS !== "WINNT") {
      for (const path of [launcher, executable]) {
        await IOUtils.setPermissions(path, 0o644);
        await repair();
        const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(path);
        assert(file.isExecutable(), "repair restores executable permission");
      }
      assertEquals(
        support.signed,
        5,
        "permission repair signs each replacement",
      );
    }
    assertEquals(
      await IOUtils.readUTF8(marker),
      originalMarker,
      "ownership marker remains unchanged",
    );
    const signed = support.signed;
    await repair();
    assertEquals(support.signed, signed, "healthy bundle is not re-signed");

    const capabilities = {
      protocolVersion: 1 as const,
      authenticatedTransport: true,
      nativeWindowOwnership: true,
      sharedBrowserProfile: true,
      transactionalInstall: true,
    };
    await support.prepareAppShimRegistration(app, capabilities);
    assertEquals(support.registered, 1, "intact v4 can enroll");
    for (const path of [
      executable,
      PathUtils.join(contents, "Resources", "Scripts", "main.scpt"),
      PathUtils.join(contents, "Resources", "app.icns"),
    ]) {
      const original = await IOUtils.read(path);
      await IOUtils.write(path, new Uint8Array([99, 111, 114, 114, 117, 112, 116]));
      let refused = false;
      try {
        await support.prepareAppShimRegistration(app, capabilities);
      } catch {
        refused = true;
      }
      assert(refused, "damaged v4 seal blocks registry enrollment");
      assertEquals(support.registered, 1, "damaged bundle cannot reach registry");
      const previous = support.signed;
      await repair();
      assertEquals(support.signed, previous + 1, "mock seal damage triggers replacement");
      assertEquals(
        Array.from(await IOUtils.read(path)).join(","),
        Array.from(original).join(","),
        "replacement restores damaged bytes",
      );
    }
    const before = await IOUtils.readUTF8(marker);
    const beforeExecutable = Array.from(await IOUtils.read(executable)).join(",");
    const beforeSigned = support.signed;
    support.verificationError = true;
    let rejected = false;
    try {
      await support.repair(app, store);
    } catch {
      rejected = true;
    }
    assert(rejected, "unavailable verifier rejects explicit repair");
    await support.repairExistingLaunchers(store);
    assertEquals(support.signed, beforeSigned, "unavailable verifier never rebuilds");
    assertEquals(await IOUtils.readUTF8(marker), before, "marker unchanged on verifier error");
    assertEquals(Array.from(await IOUtils.read(executable)).join(","), beforeExecutable, "applet unchanged");
    assert(!await IOUtils.exists(`${bundle.path}.floorp-backup`), "no backup created on verifier error");
    let enrollmentRejected = false;
    try {
      await support.prepareAppShimRegistration(app, capabilities);
    } catch {
      enrollmentRejected = true;
    }
    assert(enrollmentRejected, "unavailable verifier blocks enrollment");
    assertEquals(storeWrites, 0, "repair never writes the SSB store");
    const backup = `${bundle.path}.floorp-backup`;
    await IOUtils.move(bundle.path, backup);
    try {
      let recoveryRejected = false;
      try { await support.repair(app, store); } catch { recoveryRejected = true; }
      assert(recoveryRejected, "unavailable verifier rejects backup recovery");
      assert(await IOUtils.exists(backup), "backup remains in place on verifier failure");
      assert(!await IOUtils.exists(bundle.path), "backup was not moved on verifier failure");
    } finally {
      await IOUtils.move(backup, bundle.path);
    }
  } finally {
    await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
  }
}

async function startupMigrationAndOwnership(): Promise<void> {
  const root = PathUtils.join(PathUtils.tempDir, "m-" + crypto.randomUUID().slice(0, 12));
  const options: MacAppOptions = {
    applicationsDir: PathUtils.join(root, "a"),
    profileDir: PathUtils.join(root, "p"),
    executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
  };
  const support = new RepairSupport(options);
  const bundle = await getMacAppBundle(app, options);
  const contents = PathUtils.join(bundle.path, "Contents");
  const marker = PathUtils.join(contents, "floorp.json");
  const dataFile = PathUtils.join(options.profileDir, "ssb", "ssb.json");
  const data = JSON.stringify({ preserved: app });
  const store = {
    getCurrentSsbData: () => Promise.resolve({ [app.id]: app }),
    saveSsbData: () => Promise.resolve(),
    removeSsbData: () => Promise.resolve(),
  };
  try {
    await IOUtils.makeDirectory(PathUtils.parent(dataFile), { createAncestors: true });
    await IOUtils.writeUTF8(dataFile, data);
    await support.install(app);
    const prior = await IOUtils.readJSON(marker) as Record<string, unknown>;
    await IOUtils.writeJSON(marker, { ...prior, version: 3 });
    await IOUtils.remove(PathUtils.join(contents, "MacOS", "applet"));
    await IOUtils.remove(PathUtils.join(contents, "Resources", "Scripts"), { recursive: true });
    await IOUtils.move(PathUtils.join(contents, "Resources", "launcher.sh"),
      PathUtils.join(contents, "MacOS", "launcher"));
    await IOUtils.writeUTF8(PathUtils.join(contents, "Info.plist"), bundle.plist.replace(
      "<key>CFBundleExecutable</key><string>applet</string>",
      "<key>CFBundleExecutable</key><string>launcher</string>",
    ));
    await support.prepareAppShimRegistration(app, {
      protocolVersion: 1, authenticatedTransport: true, nativeWindowOwnership: true,
      sharedBrowserProfile: true, transactionalInstall: true,
    });
    assertEquals(support.registered, 1, "v3 legacy enrollment remains eligible");
    await support.repairExistingLaunchers(store);
    const updated = await IOUtils.readJSON(marker) as { version: number };
    assertEquals(updated.version, 4, "startup upgrades v3 to v4");
    assert(await IOUtils.exists(PathUtils.join(contents, "MacOS", "applet")), "applet restored");
    assertEquals(await IOUtils.readUTF8(dataFile), data, "user store unchanged");
    assertEquals(support.signed, 2, "exactly one replacement is signed");
    await support.repairExistingLaunchers(store);
    assertEquals(support.signed, 2, "second startup does not rebuild");
    await IOUtils.remove(marker);
    const sentinel = PathUtils.join(contents, "sentinel");
    await IOUtils.writeUTF8(sentinel, "unowned");
    await support.repairExistingLaunchers(store);
    assertEquals(await IOUtils.readUTF8(sentinel), "unowned", "unowned bundle preserved");
    assertEquals(support.signed, 2, "unowned bundle not replaced");
  } finally {
    await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
  }
}

class VerifyClassifierSupport extends MacOSSupport {
  response: { exitCode: number; output: string } | null = null;
  protected override runVerificationCommand(): Promise<{ exitCode: number; output: string }> {
    if (!this.response) return Promise.reject(new Error("mock spawn error"));
    return Promise.resolve(this.response);
  }
  check(path = "/private/test.app"): Promise<"valid" | "invalid"> {
    return this.verifyApplet(path);
  }
}

async function verifierClassification(): Promise<void> {
  const support = new VerifyClassifierSupport();
  support.response = { exitCode: 0, output: "" };
  assertEquals(await support.check(), "valid", "zero status is valid");
  support.response = { exitCode: 1, output: "/private/test.app: a sealed resource is missing or invalid\n" };
  assertEquals(await support.check(), "invalid", "known seal damage is invalid");
  const wrapped = "/private/test.app: invalid signature (code or signature have been modified)";
  for (const output of [wrapped, `${wrapped}\nIn architecture: x86_64\n`,
    `${wrapped}\nIn architecture: arm64\n`]) {
    support.response = { exitCode: 1, output };
    assertEquals(await support.check(), "invalid", "observed applet signature failure is invalid");
  }
  for (const response of [null,
    { exitCode: 2, output: `${wrapped}\nIn architecture: x86_64\n` },
    { exitCode: 1, output: `${wrapped}\nIn architecture: unknown\n` },
    { exitCode: 1, output: `${wrapped}\nIn architecture: x86_64\nextra\n` },
    { exitCode: 1, output: `${wrapped}\nIn architecture: x86_64\n\n` },
    { exitCode: 1, output: `noise\n${wrapped}\nIn architecture: x86_64\n` },
    { exitCode: 1, output: wrapped.replace("/private/test.app", "/private/other.app") }, { exitCode: 1, output: "Permission denied" },
    { exitCode: 2, output: "code or signature have been modified" },
    { exitCode: 1, output: "/private/test.app: Permission denied\n/private/test.app: a sealed resource is missing or invalid" },
    { exitCode: 1, output: "/tmp/code or signature have been modified.app: Permission denied" }]) {
    support.response = response;
    let rejected = false;
    try { await support.check(); } catch { rejected = true; }
    assert(rejected, "spawn and unrecognized failures remain operational errors");
  }
  const deceptivePath = "/private/a sealed resource is missing or invalid.app";
  support.response = { exitCode: 1, output: `${deceptivePath}: Permission denied` };
  let pathRejected = false;
  try { await support.check(deceptivePath); } catch { pathRejected = true; }
  assert(pathRejected, "diagnostic-looking path cannot authorize replacement");
}

export async function runAllTests(): Promise<void> {
  await runTests("MacOS.repair.test.mts", [
    { name: "codesign verifier classification (transport mocked)", fn: verifierClassification },
    {
      name: "explicit repair regenerates damaged v4 bundle",
      fn: () => runRepair(false),
    },
    {
      name: "startup repair regenerates damaged v4 bundle",
      fn: () => runRepair(true),
    },
    {
      name: "startup migrates v3 and preserves unowned bundles",
      fn: startupMigrationAndOwnership,
    },
  ]);
}
