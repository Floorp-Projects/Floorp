// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getMacAppBundle,
  type MacAppOptions,
  MacOSSupport,
  pngToIcns,
} from "../MacOS.sys.mts";
import type { Manifest } from "../../type.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const ssb: Manifest = {
  id: "{12345678-1234-5678-1234-567812345678}",
  name: "A & B <App> / 日本語",
  start_url: "https://example.com/",
  icon: "",
};

class TestMacOSSupport extends MacOSSupport {
  iconWrites = 0;
  protected override createIcon(): Promise<Uint8Array> {
    this.iconWrites++;
    return Promise.resolve(pngToIcns(new Uint8Array([1, 2, 3])));
  }

  renderIcon(manifest: Manifest): Promise<Uint8Array> {
    return super.createIcon(manifest);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("MacOS.test.mts", [
    {
      name:
        "bundle metadata escapes XML and shell arguments and isolates profiles",
      async fn() {
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(PathUtils.tempDir, "Floorp Apps"),
          profileDir: "/Users/O'Brien/Library/Profile $(touch nope)",
          executable: "/Applications/Floorp Nightly.app/Contents/MacOS/floorp",
        };
        const bundle = await getMacAppBundle(ssb, options);
        const parsed = new DOMParser().parseFromString(
          bundle.plist,
          "text/xml",
        );
        assertEquals(
          parsed.querySelector("parsererror"),
          null,
          "valid plist XML",
        );
        const strings = Array.from(
          parsed.querySelectorAll("string"),
          (node) => node.textContent,
        );
        assert(
          strings.includes(ssb.name),
          "display name survives XML encoding",
        );
        assert(
          bundle.launcher.includes("'--start-ssb'"),
          "uses SSB command handler",
        );
        assert(
          bundle.launcher.includes("'--profile'"),
          "selects installing profile",
        );
        assert(
          bundle.launcher.includes("O'\"'\"'Brien"),
          "quotes apostrophes safely",
        );
        assert(
          bundle.launcher.includes("$(touch nope)'"),
          "substitution stays quoted",
        );
        assertEquals(
          PathUtils.parent(bundle.path),
          options.applicationsDir,
          "safe child path",
        );
        const other = await getMacAppBundle(ssb, {
          ...options,
          profileDir: "/other",
        });
        assert(
          bundle.path !== other.path,
          "copied IDs in different profiles do not collide",
        );
        assertEquals(
          (await getMacAppBundle(ssb, options)).path,
          bundle.path,
          "stable path",
        );
      },
    },
    {
      name:
        "install repairs legacy apps, is idempotent, supports rename and owned removal",
      async fn() {
        const root = PathUtils.join(
          PathUtils.tempDir,
          `floorp-mac-test-${crypto.randomUUID()}`,
        );
        const options: MacAppOptions = {
          applicationsDir: PathUtils.join(root, "Applications"),
          profileDir: PathUtils.join(root, "profile"),
          executable: "/Applications/Floorp.app/Contents/MacOS/floorp",
        };
        const support = new TestMacOSSupport(options);
        // Bundle file operations run on all hosts; XML/name escaping is covered
        // separately above with characters that Windows filenames cannot hold.
        const app = { ...ssb, name: "Portable App 日本語" };
        const { path } = await getMacAppBundle(app, options);
        try {
          // An existing ssb.json entry has no OS files before this installation.
          await Promise.all([support.install(app), support.install(app)]);
          assertEquals(
            support.iconWrites,
            1,
            "concurrent/repeated launch does not rewrite app",
          );
          const contents = PathUtils.join(path, "Contents");
          const launcher = PathUtils.join(contents, "MacOS", "launcher");
          assert(
            (await IOUtils.readUTF8(launcher)).startsWith("#!/bin/sh\nexec "),
            "executable launcher",
          );
          assert(
            await IOUtils.exists(PathUtils.join(contents, "Info.plist")),
            "Finder metadata",
          );
          const icon = PathUtils.join(contents, "Resources", "app.icns");
          const bytes = await IOUtils.read(icon);
          assertEquals(
            new TextDecoder().decode(bytes.slice(0, 4)),
            "icns",
            "icon container",
          );
          assertEquals(
            new DataView(bytes.buffer).getUint32(4),
            bytes.length,
            "ICNS total size",
          );
          await IOUtils.remove(icon);
          await support.install(app);
          assertEquals(support.iconWrites, 2, "partial bundle is repaired");
          const marker = PathUtils.join(contents, "floorp.json");
          const owned = await IOUtils.readUTF8(marker);
          await IOUtils.writeJSON(marker, {
            ssb,
            profileDir: "another-profile",
          });
          let rejected = false;
          try {
            await support.uninstall(app);
          } catch {
            rejected = true;
          }
          assert(rejected, "refuses to remove another profile's bundle");
          assert(await IOUtils.exists(path), "unowned bundle remains");
          await IOUtils.writeUTF8(marker, owned);
          await support.uninstall(app);
          const renamed = { ...app, name: "Renamed" };
          await support.install(renamed);
          assert(!await IOUtils.exists(path), "old bundle removed on rename");
          const renamedPath = (await getMacAppBundle(renamed, options)).path;
          assert(await IOUtils.exists(renamedPath), "renamed bundle created");
          await support.uninstall(renamed);
          await support.uninstall(renamed);
          assert(!await IOUtils.exists(renamedPath), "uninstall is idempotent");
        } finally {
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
      },
    },
    {
      name: "missing and invalid site icons produce a valid 512px PNG in ICNS",
      async fn() {
        for (const icon of ["", "data:image/png;base64,invalid"]) {
          const bytes = await new TestMacOSSupport().renderIcon({
            ...ssb,
            icon,
          });
          assertEquals(
            new TextDecoder().decode(bytes.slice(8, 12)),
            "ic09",
            "512px ICNS element",
          );
          const png = bytes.slice(16);
          assertEquals(
            new TextDecoder().decode(png.slice(1, 4)),
            "PNG",
            "PNG signature",
          );
          const view = new DataView(png.buffer);
          assertEquals(view.getUint32(16), 512, "PNG width");
          assertEquals(view.getUint32(20), 512, "PNG height");
        }
      },
    },
  ]);
}
