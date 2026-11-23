// SPDX-License-Identifier: MPL-2.0
// File-lifecycle tests run on every OS. The real compiler/signing path is
// exercised separately by a macOS Launch Services integration test.
import { MacOSSupport } from "../MacOS.sys.mts";

export class MockMacOSSupport extends MacOSSupport {
  protected override async compileApplet(path: string): Promise<void> {
    const contents = PathUtils.join(path, "Contents");
    const executable = PathUtils.join(contents, "MacOS");
    const scripts = PathUtils.join(contents, "Resources", "Scripts");
    await IOUtils.makeDirectory(executable, {
      createAncestors: true,
      ignoreExisting: true,
    });
    await IOUtils.makeDirectory(scripts, {
      createAncestors: true,
      ignoreExisting: true,
    });
    const applet = PathUtils.join(executable, "applet");
    await IOUtils.writeUTF8(applet, "mock native applet");
    if (Services.appinfo.OS !== "WINNT") {
      await IOUtils.setPermissions(applet, 0o755);
    }
    await IOUtils.writeUTF8(
      PathUtils.join(scripts, "main.scpt"),
      "mock script",
    );
  }

  protected override signApplet(
    _path: string,
    _bundleId: string,
  ): Promise<void> {
    return Promise.resolve();
  }
}
