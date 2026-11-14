// SPDX-License-Identifier: MPL-2.0
// File-lifecycle tests run on every OS. The real compiler/signing path is
// exercised separately by a macOS Launch Services integration test.
import { MacOSSupport } from "../MacOS.sys.mts";

export class MockMacOSSupport extends MacOSSupport {
  // This is a deterministic test double, not a cryptographic signature.
  private mockSeals = new Set<string>();
  verificationError = false;

  private async mockFingerprint(path: string): Promise<string> {
    const contents = PathUtils.join(path, "Contents");
    const files = [
      PathUtils.join(contents, "MacOS", "applet"),
      PathUtils.join(contents, "Resources", "Scripts", "main.scpt"),
      PathUtils.join(contents, "Resources", "app.icns"),
    ];
    const hashes: string[] = [];
    for (const file of files) {
      const bytes = await IOUtils.read(file);
      const digest = await crypto.subtle.digest("SHA-256", bytes.buffer as ArrayBuffer);
      hashes.push(Array.from(new Uint8Array(digest)).join(","));
    }
    return hashes.join(":");
  }

  protected override async verifyApplet(path: string): Promise<"valid" | "invalid"> {
    if (this.verificationError) throw new Error("mock verifier unavailable");
    return this.mockSeals.has(await this.mockFingerprint(path)) ? "valid" : "invalid";
  }
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

  protected override async signApplet(
    path: string,
    _bundleId: string,
  ): Promise<void> {
    this.mockSeals.add(await this.mockFingerprint(path));
  }
}
