// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getMacAppBundle, type MacAppOptions } from "../MacOS.sys.mts";
import { MockMacOSSupport } from "./MockMacOSSupport.ts";
import type { Manifest } from "../../type.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const app: Manifest = {
  id: "{12345678-1234-5678-1234-567812345678}",
  name: "Repair Fixture",
  start_url: "https://example.com/",
  icon: "",
};

class RepairSupport extends MockMacOSSupport {
  compiled = 0;
  signed = 0;
  protected override async compileApplet(path: string): Promise<void> {
    this.compiled++;
    await super.compileApplet(path);
  }
  protected override signApplet(): Promise<void> {
    this.signed++;
    return Promise.resolve();
  }
  protected override createIcon(): Promise<Uint8Array> {
    return Promise.resolve(new Uint8Array([1, 2, 3]));
  }
}

async function runRepair(startup: boolean): Promise<void> {
  const root = PathUtils.join(
    PathUtils.tempDir,
    `floorp-pwa-repair-${crypto.randomUUID()}`,
  );
  const options: MacAppOptions = {
    applicationsDir: PathUtils.join(root, "Applications"),
    profileDir: PathUtils.join(root, "profile"),
    executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
  };
  const support = new RepairSupport(options);
  const bundle = await getMacAppBundle(app, options);
  const contents = PathUtils.join(bundle.path, "Contents");
  const launcher = PathUtils.join(contents, "Resources", "launcher.sh");
  const executable = PathUtils.join(contents, "MacOS", "applet");
  const plist = PathUtils.join(contents, "Info.plist");
  const marker = PathUtils.join(contents, "floorp.json");
  const store = {
    getCurrentSsbData: () => Promise.resolve({ [app.id]: app }),
    saveSsbData: () => Promise.resolve(),
    removeSsbData: () => Promise.resolve(),
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
  } finally {
    await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("MacOS.repair.test.mts", [
    {
      name: "explicit repair regenerates damaged v4 bundle",
      fn: () => runRepair(false),
    },
    {
      name: "startup repair regenerates damaged v4 bundle",
      fn: () => runRepair(true),
    },
  ]);
}
