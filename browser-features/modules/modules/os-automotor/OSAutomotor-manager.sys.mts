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
const GITHUB_FRONTEND_RELEASE_URL =
  "https://github.com/Floorp-Projects/Floorp-OS-Automator-Frontend/releases/download";
const FLOORP_OS_ENABLED_PREF = "floorp.os.enabled";
const FLOORP_OS_BINARY_PATH_PREF = "floorp.os.binaryPath";
const FLOORP_OS_VERSION_PREF = "floorp.os.version";
const CURRENT_VERSION = "v0.5.6-alpha";
const CURRENT_FRONTEND_VERSION = "v0.0.2";
const FLOORP_FRONTEND_BINARY_PATH_PREF = "floorp.os.frontendBinaryPath";
const FLOORP_FRONTEND_VERSION_PREF = "floorp.os.frontendVersion";

interface PlatformInfo {
  supported: boolean;
  binaryName: string;
  frontendBinaryName?: string;
}

class OSAutomotorManager {
  private _initialized = false;
  private _binaryProcess: nsIProcess | null = null;
  private _frontendProcess: nsIProcess | null = null;

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
      // Ensure and start frontend web server
      try {
        await this.ensureFrontendInstalled();
        await this.startFrontend();
      } catch (e) {
        console.error("[Floorp OS] Failed to initialize frontend:", e);
      }
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
        binaryName: `Sapphillon-Controller-v0.5.6-alpha-x86_64-pc-windows-msvc.exe`,
        frontendBinaryName: `sapphillon-front-web-server-${CURRENT_FRONTEND_VERSION}-x86_64-pc-windows-msvc.exe`,
      };
    }

    // Unsupported platform
    return {
      supported: false,
      binaryName: "",
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
   * Get the path to the frontend binary in the profile directory
   */
  private getFrontendBinaryPath(): string {
    const platformInfo = this.getPlatformInfo();
    const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    const floorpOSDir = PathUtils.join(profileDir, "floorp-os");
    const frontName = platformInfo.frontendBinaryName || "";
    const frontendPath = PathUtils.join(floorpOSDir, frontName);
    return frontendPath;
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

    const downloadUrl = `https://r2.floorp.app/${platformInfo.binaryName}`;
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
      // On failure during setup, reset installation to initial state
      try {
        await this.resetInstallation();
      } catch (e) {
        console.error(
          "[Floorp OS] Failed to reset after binary download error:",
          e,
        );
      }
      throw error;
    }
  }

  /**
   * Download the frontend web-server binary from releases
   */
  private async downloadFrontend(): Promise<void> {
    const platformInfo = this.getPlatformInfo();

    if (!platformInfo.supported || !platformInfo.frontendBinaryName) {
      console.error(
        "[Floorp OS] Current platform does not support frontend or no frontend name",
      );
      throw new Error("Frontend not supported");
    }

    const downloadUrl = `${GITHUB_FRONTEND_RELEASE_URL}/${CURRENT_FRONTEND_VERSION}/${platformInfo.frontendBinaryName}`;
    const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    const floorpOSDir = PathUtils.join(profileDir, "floorp-os");
    const frontendPath = this.getFrontendBinaryPath();

    try {
      if (!(await IOUtils.exists(floorpOSDir))) {
        await IOUtils.makeDirectory(floorpOSDir);
      }

      const response = await fetch(downloadUrl);
      if (!response.ok) {
        const errorMsg = `Failed to download frontend: HTTP ${response.status} ${response.statusText}`;
        console.error(`[Floorp OS] ${errorMsg}`);
        console.error(`[Floorp OS] Download URL: ${downloadUrl}`);
        throw new Error(errorMsg);
      }

      const arrayBuffer = await response.arrayBuffer();
      const uint8Array = new Uint8Array(arrayBuffer);

      await IOUtils.write(frontendPath, uint8Array, {
        mode: "create",
      });

      if (AppConstants.platform !== "win") {
        await IOUtils.setPermissions(frontendPath, 0o755);
      }

      Services.prefs.setStringPref(
        FLOORP_FRONTEND_BINARY_PATH_PREF,
        frontendPath,
      );
      Services.prefs.setStringPref(
        FLOORP_FRONTEND_VERSION_PREF,
        CURRENT_FRONTEND_VERSION,
      );
    } catch (error) {
      console.error("[Floorp OS] Failed to download frontend:", error);
      // On failure during setup, reset installation to initial state
      try {
        await this.resetInstallation();
      } catch (e) {
        console.error(
          "[Floorp OS] Failed to reset after frontend download error:",
          e,
        );
      }
      throw error;
    }
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
    // Ensure frontend as well
    // Only attempt frontend install on supported platforms that define a frontend name
    const platformInfo = this.getPlatformInfo();
    if (platformInfo.frontendBinaryName) {
      await this.ensureFrontendInstalled();
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
      const args: string[] = ["start"];
      process.runAsync(args, args.length);

      this._binaryProcess = process;
    } catch (error) {
      console.error("[Floorp OS] Failed to start binary:", error);
      throw error;
    }
  }

  /**
   * Start the frontend web server binary
   */
  private async startFrontend(): Promise<void> {
    if (this._frontendProcess) {
      return;
    }

    const frontendPath = this.getFrontendBinaryPath();
    const frontendExists = await IOUtils.exists(frontendPath);

    if (!frontendExists) {
      console.error("[Floorp OS] Frontend binary not found at:", frontendPath);
      throw new Error("Frontend binary not found");
    }

    try {
      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(frontendPath);

      const process = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess,
      );
      process.init(file);

      // Start the frontend in background; some frontends may accept 'start'
      const args: string[] = ["start"];
      process.runAsync(args, args.length);

      this._frontendProcess = process;
    } catch (error) {
      console.error("[Floorp OS] Failed to start frontend:", error);
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
    // Stop frontend if running
    this.stopFrontend();
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

      // Start frontend if available
      try {
        await this.startFrontend();
      } catch (e) {
        // Non-fatal for enabling OS — log and continue; we still mark enabled only when core succeeded
        console.error("[Floorp OS] Failed to start frontend during enable:", e);
      }

      // Only set enabled to true if everything succeeded
      Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, true);
    } catch (error) {
      // Make sure enabled is set to false on error
      Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, false);
      console.error("[Floorp OS] Failed to enable:", error);
      try {
        await this.resetInstallation();
      } catch (e) {
        console.error("[Floorp OS] Failed to reset after enable error:", e);
      }
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
   * Ensure frontend binary is installed and up-to-date
   */
  private async ensureFrontendInstalled(): Promise<void> {
    const frontendPath = this.getFrontendBinaryPath();
    const storedVersion = Services.prefs.getStringPref(
      FLOORP_FRONTEND_VERSION_PREF,
      "",
    );

    const frontendExists = await IOUtils.exists(frontendPath);

    if (!frontendExists || storedVersion !== CURRENT_FRONTEND_VERSION) {
      await this.downloadFrontend();
    }
  }

  /**
   * Stop the frontend process
   */
  public stopFrontend(): void {
    if (this._frontendProcess) {
      try {
        this._frontendProcess.kill();
        this._frontendProcess = null;
      } catch (error) {
        console.error("[Floorp OS] Failed to stop frontend:", error);
      }
    }
  }

  /**
   * Reset installation state: stop processes, remove files/dirs, clear prefs
   */
  private async resetInstallation(): Promise<void> {
    // Stop any running processes first
    try {
      this.stopFrontend();
    } catch (e) {
      console.error("[Floorp OS] Error stopping frontend during reset:", e);
    }

    try {
      this.stopFloorpOS();
    } catch (e) {
      console.error("[Floorp OS] Error stopping core binary during reset:", e);
    }

    // Remove files and directory under profile
    try {
      const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
      const floorpOSDir = PathUtils.join(profileDir, "floorp-os");

      // Remove frontend binary if exists
      try {
        const frontendPath = this.getFrontendBinaryPath();
        await IOUtils.remove(frontendPath, { ignoreAbsent: true });
      } catch (e) {
        // Continue even if specific file removal fails
        console.error(
          "[Floorp OS] Failed to remove frontend file during reset:",
          e,
        );
      }

      // Remove core binary
      try {
        const binaryPath = this.getBinaryPath();
        await IOUtils.remove(binaryPath, { ignoreAbsent: true });
      } catch (e) {
        console.error(
          "[Floorp OS] Failed to remove core binary during reset:",
          e,
        );
      }

      // Remove the containing directory recursively
      try {
        await IOUtils.remove(floorpOSDir, {
          recursive: true,
          ignoreAbsent: true,
        });
      } catch (e) {
        console.error(
          "[Floorp OS] Failed to remove floorp-os directory during reset:",
          e,
        );
      }
    } catch (e) {
      console.error("[Floorp OS] Error during filesystem cleanup:", e);
    }

    // Clear preferences
    try {
      try {
        Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, false);
      } catch {
        // ignore
      }

      try {
        if (Services.prefs.prefHasUserValue(FLOORP_OS_BINARY_PATH_PREF)) {
          Services.prefs.clearUserPref(FLOORP_OS_BINARY_PATH_PREF);
        }
      } catch (e) {
        console.error("[Floorp OS] Failed to clear binary path pref:", e);
      }

      try {
        if (Services.prefs.prefHasUserValue(FLOORP_OS_VERSION_PREF)) {
          Services.prefs.clearUserPref(FLOORP_OS_VERSION_PREF);
        }
      } catch (e) {
        console.error("[Floorp OS] Failed to clear binary version pref:", e);
      }

      try {
        if (Services.prefs.prefHasUserValue(FLOORP_FRONTEND_BINARY_PATH_PREF)) {
          Services.prefs.clearUserPref(FLOORP_FRONTEND_BINARY_PATH_PREF);
        }
      } catch (e) {
        console.error("[Floorp OS] Failed to clear frontend path pref:", e);
      }

      try {
        if (Services.prefs.prefHasUserValue(FLOORP_FRONTEND_VERSION_PREF)) {
          Services.prefs.clearUserPref(FLOORP_FRONTEND_VERSION_PREF);
        }
      } catch (e) {
        console.error("[Floorp OS] Failed to clear frontend version pref:", e);
      }
    } catch (e) {
      console.error("[Floorp OS] Error clearing prefs during reset:", e);
    }
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
