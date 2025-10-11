/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * OSAutomotor-manager - A service for initializing Floorp OS Backend additional modules
 *
 * This module will check the OS modules installed and install missing modules.
 * It downloads the Sapphillon Controller binary from GitHub releases and manages its lifecycle.
 */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

// Type for nsIProcess
type nsIProcess = {
  init(executable: nsIFile): void;
  runAsync(args: string[], numArgs: number): void;
  kill(): void;
};

// Type for nsIFile
type nsIFile = {
  initWithPath(path: string): void;
};

const GITHUB_RELEASE_URL =
  "https://github.com/Walkmana-25/Sapphillon/releases/latest/download";
const FLOORP_OS_ENABLED_PREF = "floorp.os.enabled";
const FLOORP_OS_BINARY_PATH_PREF = "floorp.os.binaryPath";
const FLOORP_OS_VERSION_PREF = "floorp.os.version";
const CURRENT_VERSION = "v0.5.6-alpha";

interface PlatformInfo {
  supported: boolean;
  binaryName: string;
  sha256: string;
}

class OSAutomotorManager {
  private _initialized = false;
  private _binaryProcess: nsIProcess | null = null;

  constructor() {
    if (this._initialized) {
      return;
    }

    this.initialize();
    this._initialized = true;
  }

  /**
   * Initialize the OSAutomotor manager
   */
  private async initialize(): Promise<void> {
    // Check if Floorp OS is enabled
    const isEnabled = Services.prefs.getBoolPref(FLOORP_OS_ENABLED_PREF, false);

    if (isEnabled) {
      await this.ensureBinaryInstalled();
      await this.startFloorpOS();
    }
  }

  /**
   * Get platform information for the current system
   */
  private getPlatformInfo(): PlatformInfo {
    const os = AppConstants.platform;
    const arch = String(Services.sysinfo.get("arch"));

    // Windows x86_64 (support multiple arch strings)
    if (
      os === "win" &&
      (arch === "x86_64" ||
        arch === "x86-64" ||
        arch === "x64" ||
        arch.toLowerCase().includes("amd64"))
    ) {
      return {
        supported: true,
        binaryName: `Sapphillon-Controller-${CURRENT_VERSION}-x86_64-pc-windows-msvc.exe`,
        sha256:
          "00f5e20b72670bbb9e44e95e9ae8a9ea009d0a96ba3b5594067ae3129e782e37",
      };
    }

    // Linux x86_64 (support multiple arch strings)
    if (
      os === "linux" &&
      (arch === "x86_64" ||
        arch === "x86-64" ||
        arch === "x64" ||
        arch.toLowerCase().includes("amd64"))
    ) {
      return {
        supported: true,
        binaryName: `Sapphillon-Controller-${CURRENT_VERSION}-x86_64-unknown-linux-gnu`,
        sha256:
          "d5236d522e6ff024d739ee209669381b27bfd631cbb7a3251cfbc396aa62b0dd",
      };
    }

    // Linux aarch64
    if (
      os === "linux" &&
      (arch === "aarch64" ||
        arch === "arm64" ||
        arch.toLowerCase().includes("arm"))
    ) {
      return {
        supported: true,
        binaryName: `Sapphillon-Controller-${CURRENT_VERSION}-aarch64-unknown-linux-gnu`,
        sha256:
          "edbd4a216a838acf719684ed5efbc9c79931990321c97347cedaf36aefb5bf90",
      };
    }

    // macOS aarch64 (Apple Silicon)
    if (
      os === "macosx" &&
      (arch === "aarch64" ||
        arch === "arm64" ||
        arch.toLowerCase().includes("arm"))
    ) {
      return {
        supported: true,
        binaryName: `Sapphillon-Controller-${CURRENT_VERSION}-aarch64-apple-darwin`,
        sha256:
          "7c573a9c423b2bc2dfd1f2ed31034a3212aefafcd9b2055888b842ae78e9b232",
      };
    }

    // Unsupported platform
    return {
      supported: false,
      binaryName: "",
      sha256: "",
    };
  }

  /**
   * Get the path to the binary in the profile directory
   */
  private getBinaryPath(): string {
    const platformInfo = this.getPlatformInfo();
    const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    const floorpOSDir = PathUtils.join(profileDir, "floorp-os");
    const binaryPath = PathUtils.join(floorpOSDir, platformInfo.binaryName);
    return binaryPath;
  }

  /**
   * Check if the platform is supported
   */
  public isPlatformSupported(): boolean {
    return this.getPlatformInfo().supported;
  }

  /**
   * Download the binary from GitHub releases
   */
  private async downloadBinary(): Promise<void> {
    const platformInfo = this.getPlatformInfo();

    if (!platformInfo.supported) {
      console.error("[Floorp OS] Current platform is not supported");
      throw new Error("Platform not supported");
    }

    const downloadUrl = `${GITHUB_RELEASE_URL}/${platformInfo.binaryName}`;
    const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    const floorpOSDir = PathUtils.join(profileDir, "floorp-os");
    const binaryPath = this.getBinaryPath();

    try {
      // Create floorp-os directory if it doesn't exist
      if (!(await IOUtils.exists(floorpOSDir))) {
        await IOUtils.makeDirectory(floorpOSDir);
      }

      // Download the binary
      const response = await fetch(downloadUrl);
      if (!response.ok) {
        const errorMsg = `Failed to download: HTTP ${response.status} ${response.statusText}`;
        console.error(`[Floorp OS] ${errorMsg}`);
        console.error(`[Floorp OS] Download URL: ${downloadUrl}`);
        throw new Error(errorMsg);
      }

      const arrayBuffer = await response.arrayBuffer();
      const uint8Array = new Uint8Array(arrayBuffer);

      // Write the binary to the profile directory
      await IOUtils.write(binaryPath, uint8Array, {
        mode: "create",
      });

      // Verify the SHA256 hash
      const hash = await this.calculateSHA256(uint8Array);
      if (hash !== platformInfo.sha256) {
        console.error(
          `[Floorp OS] SHA256 mismatch! Expected: ${platformInfo.sha256}, Got: ${hash}`,
        );
        // Delete the corrupted file
        await IOUtils.remove(binaryPath);
        throw new Error("SHA256 verification failed");
      }

      // Make the binary executable on Unix-like systems
      if (AppConstants.platform !== "win") {
        await IOUtils.setPermissions(binaryPath, 0o755);
      }

      // Store the binary path in preferences
      // Store the binary path in preferences
      Services.prefs.setStringPref(FLOORP_OS_BINARY_PATH_PREF, binaryPath);
      Services.prefs.setStringPref(FLOORP_OS_VERSION_PREF, CURRENT_VERSION);
    } catch (error) {
      console.error("[Floorp OS] Failed to download binary:", error);
      throw error;
    }
  }

  /**
   * Calculate SHA256 hash of the binary
   */
  private async calculateSHA256(data: Uint8Array): Promise<string> {
    const hashBuffer = await crypto.subtle.digest("SHA-256", data);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    const hashHex = hashArray
      .map((b) => b.toString(16).padStart(2, "0"))
      .join("");
    return hashHex;
  }

  /**
   * Check if the binary is installed and up-to-date
   */
  private async ensureBinaryInstalled(): Promise<void> {
    const binaryPath = this.getBinaryPath();
    const storedVersion = Services.prefs.getStringPref(
      FLOORP_OS_VERSION_PREF,
      "",
    );

    // Check if binary exists and version matches
    const binaryExists = await IOUtils.exists(binaryPath);

    if (!binaryExists || storedVersion !== CURRENT_VERSION) {
      await this.downloadBinary();
    }
  }

  /**
   * Start the Floorp OS binary
   */
  private async startFloorpOS(): Promise<void> {
    if (this._binaryProcess) {
      return;
    }

    const binaryPath = this.getBinaryPath();
    const binaryExists = await IOUtils.exists(binaryPath);

    if (!binaryExists) {
      const error = new Error("Binary not found");
      console.error("[Floorp OS] Binary not found at:", binaryPath);
      throw error;
    }

    try {
      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(binaryPath);

      const process = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess,
      );
      process.init(file);

      // Start the process in the background
      const args: string[] = [];
      process.runAsync(args, args.length);

      this._binaryProcess = process;
    } catch (error) {
      console.error("[Floorp OS] Failed to start binary:", error);
      throw error;
    }
  }

  /**
   * Stop the Floorp OS binary
   */
  public stopFloorpOS(): void {
    if (this._binaryProcess) {
      try {
        this._binaryProcess.kill();
        this._binaryProcess = null;
      } catch (error) {
        console.error("[Floorp OS] Failed to stop binary:", error);
      }
    }
  }

  /**
   * Enable Floorp OS
   */
  public async enableFloorpOS(): Promise<void> {
    if (!this.isPlatformSupported()) {
      throw new Error("Platform not supported");
    }

    try {
      // Download and verify binary first
      await this.ensureBinaryInstalled();

      // Start the binary
      await this.startFloorpOS();

      // Only set enabled to true if everything succeeded
      Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, true);
    } catch (error) {
      // Make sure enabled is set to false on error
      Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, false);
      console.error("[Floorp OS] Failed to enable:", error);
      throw error;
    }
  }

  /**
   * Disable Floorp OS
   */
  public disableFloorpOS(): void {
    Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, false);
    this.stopFloorpOS();
  }

  /**
   * Check if Floorp OS is enabled
   */
  public isEnabled(): boolean {
    return Services.prefs.getBoolPref(FLOORP_OS_ENABLED_PREF, false);
  }

  /**
   * Get the installed version
   */
  public getInstalledVersion(): string {
    return Services.prefs.getStringPref(FLOORP_OS_VERSION_PREF, "");
  }

  /**
   * Get platform debug information
   */
  public getPlatformDebugInfo(): {
    os: string;
    arch: string;
    supported: boolean;
  } {
    const os = AppConstants.platform;
    const arch = String(Services.sysinfo.get("arch"));
    const platformInfo = this.getPlatformInfo();
    return {
      os,
      arch,
      supported: platformInfo.supported,
    };
  }
}

/**
 * Singleton instance of OSAutomotorManager.
 * Importing this module will automatically initialize the manager.
 */
export const osAutomotorManager = new OSAutomotorManager();
