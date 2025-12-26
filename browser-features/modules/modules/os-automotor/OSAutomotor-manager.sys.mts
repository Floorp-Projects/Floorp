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

type SubprocessStdOption = "pipe";

interface SubprocessResult {
  exitCode: number;
  signal?: number | null;
}

interface SubprocessPipe {
  readString(): Promise<string | null>;
}

interface SubprocessCallOptions {
  command: string;
  arguments?: string[];
  stdout?: SubprocessStdOption;
  stderr?: SubprocessStdOption;
  environment?: Record<string, string>;
}

interface SubprocessProcess {
  stdout?: SubprocessPipe;
  stderr?: SubprocessPipe;
  kill(): Promise<void>;
  wait(): Promise<SubprocessResult>;
  pid?: number;
}

interface SubprocessModule {
  call(options: SubprocessCallOptions): Promise<SubprocessProcess>;
}

const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs",
) as { Subprocess: SubprocessModule };

const GITHUB_FRONTEND_RELEASE_URL =
  "https://github.com/Floorp-Projects/Floorp-OS-Automator-Frontend/releases/download";
const FLOORP_OS_ENABLED_PREF = "floorp.os.enabled";
const FLOORP_OS_BINARY_PATH_PREF = "floorp.os.binaryPath";
const FLOORP_OS_VERSION_PREF = "floorp.os.version";
const CURRENT_VERSION = "v0.13.1";
const CURRENT_FRONTEND_VERSION = "v0.13.2";
const FLOORP_FRONTEND_BINARY_PATH_PREF = "floorp.os.frontendBinaryPath";
const FLOORP_FRONTEND_VERSION_PREF = "floorp.os.frontendVersion";
const FLOORP_OS_INIAD_API_KEY_PREF = "floorp.os.iniad.apiKey";
const FLOORP_OS_DIR_NAME = "floorp-os";
const RUNTIME_STATE_FILE_NAME = "runtime-state.json";

interface PlatformInfo {
  supported: boolean;
  binaryName: string;
  frontendBinaryName?: string;
}

type SpawnedProcess = Awaited<ReturnType<typeof Subprocess.call>>;
type ProcessPipe = NonNullable<SpawnedProcess["stdout"]>;
interface RuntimeState {
  corePid: number | null;
  frontendPid: number | null;
  timestamp: number;
}

class OSAutomotorManager {
  private _initialized = false;
  private _binaryProcess: SpawnedProcess | null = null;
  private _frontendProcess: SpawnedProcess | null = null;
  private _runtimeState: RuntimeState | null = null;

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
    console.log("[Floorp OS] OSAutomotor initialize starting...");
    await this.recoverFromUncleanShutdown();
    // Check if Floorp OS is enabled
    const isEnabled = Services.prefs.getBoolPref(FLOORP_OS_ENABLED_PREF, false);
    console.log(`[Floorp OS] floorp.os.enabled = ${isEnabled}`);

    if (isEnabled) {
      console.log("[Floorp OS] Starting Floorp OS...");
      await this.ensureBinaryInstalled();
      await this.startFloorpOS();
      // Ensure and start frontend web server
      try {
        await this.ensureFrontendInstalled();
        await this.startFrontend();
      } catch (e) {
        console.error("[Floorp OS] Failed to initialize frontend:", e);
      }
      console.log("[Floorp OS] Floorp OS initialization complete.");
    } else {
      console.log("[Floorp OS] Floorp OS is disabled, skipping startup.");
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
        frontendBinaryName: `sapphillon-front-web-server-${CURRENT_FRONTEND_VERSION}-x86_64-pc-windows-msvc.exe`,
      };
    }

    // macOS ARM64
    if (os === "macosx" && (arch === "aarch64" || arch === "arm64")) {
      return {
        supported: true,
        binaryName: `Sapphillon-Controller-${CURRENT_VERSION}-aarch64-apple-darwin`,
        frontendBinaryName: `sapphillon-front-web-server-${CURRENT_FRONTEND_VERSION}-aarch64-apple-darwin`,
      };
    }

    // Linux x86_64
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
        frontendBinaryName: `sapphillon-front-web-server-${CURRENT_FRONTEND_VERSION}-x86_64-unknown-linux-gnu`,
      };
    }

    // Linux ARM64
    if (os === "linux" && (arch === "aarch64" || arch === "arm64")) {
      return {
        supported: true,
        binaryName: `Sapphillon-Controller-${CURRENT_VERSION}-aarch64-unknown-linux-gnu`,
        frontendBinaryName: `sapphillon-front-web-server-${CURRENT_FRONTEND_VERSION}-aarch64-unknown-linux-gnu`,
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
    const floorpOSDir = this.getFloorpOSDirectory();
    const binaryPath = PathUtils.join(floorpOSDir, platformInfo.binaryName);
    return binaryPath;
  }

  /**
   * Get the path to the frontend binary in the profile directory
   */
  private getFrontendBinaryPath(): string {
    const platformInfo = this.getPlatformInfo();
    const floorpOSDir = this.getFloorpOSDirectory();
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

    const downloadUrl = `https://os.floorp.app/automator/${platformInfo.binaryName}`;
    const floorpOSDir = this.getFloorpOSDirectory();
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
    const floorpOSDir = this.getFloorpOSDirectory();
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
      const floorpOSDir = this.getFloorpOSDirectory();
      const dbPath = PathUtils.join(floorpOSDir, "sqlite.db");
      // Convert backslashes to forward slashes for SQLite URL format
      const dbPathNormalized = dbPath.replace(/\\/g, "/");
      const dbUrl = `sqlite://${dbPathNormalized}`;
      const environment: Record<string, string> = {
        HOME: Services.dirsvc.get("Home", Ci.nsIFile).path,
      };
      const iniadApiKey = Services.prefs.getStringPref(
        FLOORP_OS_INIAD_API_KEY_PREF,
        "",
      );
      if (iniadApiKey) {
        environment.INIAD_API_KEY = iniadApiKey;
      }

      const process = await this.spawnProcess(
        binaryPath,
        ["--db-url", dbUrl, "start"],
        "core",
        environment,
      );
      this._binaryProcess = process;
      await this.updateRuntimeState({
        corePid: this.getProcessPid(process),
      });
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
      const environment: Record<string, string> = {
        SAPPHILLON_GRPC_BASE_URL: "http://localhost:50051",
        HOME: Services.dirsvc.get("Home", Ci.nsIFile).path,
      };
      const process = await this.spawnProcess(
        frontendPath,
        ["start"],
        "frontend",
        environment,
      );
      this._frontendProcess = process;
      await this.updateRuntimeState({
        frontendPid: this.getProcessPid(process),
      });
    } catch (error) {
      console.error("[Floorp OS] Failed to start frontend:", error);
      throw error;
    }
  }

  /**
   * Stop the Floorp OS binary
   */
  /**
   * Stop the Floorp OS binary
   */
  public async stopFloorpOS(): Promise<void> {
    if (this._binaryProcess) {
      await this.killSpawnedProcess(this._binaryProcess, "core");
      this._binaryProcess = null;
    }
    await this.updateRuntimeState({ corePid: null });
    // Stop frontend if running
    await this.stopFrontend();
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
  /**
   * Disable Floorp OS
   */
  public async disableFloorpOS(): Promise<void> {
    Services.prefs.setBoolPref(FLOORP_OS_ENABLED_PREF, false);
    await this.stopFloorpOS();
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
  /**
   * Stop the frontend process
   */
  public async stopFrontend(): Promise<void> {
    if (this._frontendProcess) {
      await this.killSpawnedProcess(this._frontendProcess, "frontend");
      this._frontendProcess = null;
    }
    await this.updateRuntimeState({ frontendPid: null });
  }

  /**
   * Reset installation state: stop processes, remove files/dirs, clear prefs
   */
  private async resetInstallation(): Promise<void> {
    // Stop any running processes first
    try {
      await this.stopFrontend();
    } catch (e) {
      console.error("[Floorp OS] Error stopping frontend during reset:", e);
    }

    try {
      await this.stopFloorpOS();
    } catch (e) {
      console.error("[Floorp OS] Error stopping core binary during reset:", e);
    }

    // Remove files and directory under profile
    try {
      const floorpOSDir = this.getFloorpOSDirectory();

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
    try {
      await this.clearRuntimeState();
    } catch (e) {
      console.error(
        "[Floorp OS] Failed to clear runtime state during reset:",
        e,
      );
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
  private async spawnProcess(
    executablePath: string,
    args: string[],
    kind: "core" | "frontend",
    environment?: Record<string, string>,
  ): Promise<SpawnedProcess> {
    const process = await Subprocess.call({
      command: executablePath,
      arguments: args,
      stdout: "pipe",
      stderr: "pipe",
      environment,
    });

    const label = kind === "core" ? "Controller" : "Frontend";
    this.capturePipe(process.stdout, `${label} stdout`);
    this.capturePipe(process.stderr, `${label} stderr`);

    void process.wait().then(
      (result: SubprocessResult) => {
        const exitCode = result.exitCode;
        const message = `[Floorp OS][${label}] exited with code ${exitCode}`;
        if (exitCode === 0) {
          console.info(message);
        } else {
          console.error(message);
        }
        if (kind === "core" && this._binaryProcess === process) {
          this._binaryProcess = null;
          void this.updateRuntimeState({ corePid: null });
        }
        if (kind === "frontend" && this._frontendProcess === process) {
          this._frontendProcess = null;
          void this.updateRuntimeState({ frontendPid: null });
        }
      },
      (error: unknown) => {
        console.error(
          `[Floorp OS] Failed waiting for ${label} process:`,
          error,
        );
      },
    );

    return process;
  }

  private capturePipe(pipe: ProcessPipe | undefined, label: string): void {
    if (!pipe) {
      return;
    }

    (async () => {
      try {
        while (true) {
          const chunk = await pipe.readString();
          if (chunk === null) {
            break;
          }
          const output = chunk.trimEnd();
          if (output.length === 0) {
            continue;
          }
          console.log(`[Floorp OS][${label}] ${output}`);
        }
      } catch (error: unknown) {
        console.error(`[Floorp OS] Failed reading ${label}:`, error);
      }
    })().catch((error: unknown) => {
      console.error(`[Floorp OS] Unexpected error capturing ${label}:`, error);
    });
  }

  private async killSpawnedProcess(
    process: SpawnedProcess,
    kind: "core" | "frontend",
  ): Promise<void> {
    const label = kind === "core" ? "Controller" : "Frontend";
    try {
      await process.kill();
      await process.wait();
      console.info(`[Floorp OS] ${label} stopped successfully.`);
    } catch (error) {
      console.error(`[Floorp OS] Failed to stop ${label}:`, error);
    }
  }

  private getFloorpOSDirectory(): string {
    const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    return PathUtils.join(profileDir, FLOORP_OS_DIR_NAME);
  }

  private getRuntimeStatePath(): string {
    const floorpOSDir = this.getFloorpOSDirectory();
    return PathUtils.join(floorpOSDir, RUNTIME_STATE_FILE_NAME);
  }

  private getProcessPid(process: SpawnedProcess | null): number | null {
    if (!process) {
      return null;
    }
    const pid = process.pid;
    if (typeof pid === "number" && Number.isFinite(pid)) {
      return pid;
    }
    return null;
  }

  private async recoverFromUncleanShutdown(): Promise<void> {
    const previousState = await this.readRuntimeState();
    if (!previousState) {
      return;
    }
    await this.killProcessByPid(previousState.corePid, "core");
    await this.killProcessByPid(previousState.frontendPid, "frontend");
    await this.clearRuntimeState();
  }

  private async killProcessByPid(
    pid: number | null,
    kind: "core" | "frontend",
  ): Promise<void> {
    if (pid === null || pid <= 0) {
      return;
    }
    try {
      const currentPid = Services.appinfo.processID;
      if (pid === currentPid) {
        return;
      }
    } catch (error) {
      console.error("[Floorp OS] Failed to read current process ID:", error);
    }

    // Safe Check: Verify process name before killing
    try {
      const platformInfo = this.getPlatformInfo();
      const expectedName =
        kind === "core"
          ? platformInfo.binaryName
          : platformInfo.frontendBinaryName;

      if (!expectedName) {
        console.warn(
          `[Floorp OS] No expected binary name for ${kind}, skipping safe kill.`,
        );
        return;
      }

      const isProcessMatch = await this.verifyProcessName(pid, expectedName);
      if (!isProcessMatch) {
        console.warn(
          `[Floorp OS] Process ${pid} does not match expected name ${expectedName}. Skipping kill to prevent data loss.`,
        );
        return;
      }
    } catch (error) {
      console.error(
        `[Floorp OS] Error verifying process ${pid} for ${kind}:`,
        error,
      );
      return;
    }

    const command =
      AppConstants.platform === "win"
        ? "C:\\Windows\\System32\\taskkill.exe"
        : "kill";
    const args =
      AppConstants.platform === "win"
        ? ["/PID", String(pid), "/T", "/F"]
        : ["-TERM", String(pid)];
    try {
      const process = await Subprocess.call({
        command,
        arguments: args,
        stdout: "pipe",
        stderr: "pipe",
      });
      await process.wait();
      console.info(
        `[Floorp OS] Ensured ${kind} process (pid=${pid}) is terminated`,
      );
    } catch (error) {
      console.error(
        `[Floorp OS] Failed to terminate ${kind} process by pid ${pid}:`,
        error,
      );
    }
  }

  /**
   * Verify if the process with the given PID matches the expected binary name.
   */
  private async verifyProcessName(
    pid: number,
    expectedName: string,
  ): Promise<boolean> {
    const isWindows = AppConstants.platform === "win";

    try {
      if (isWindows) {
        // Windows: tasklist /FI "PID eq <pid>" /FO CSV /NH
        const process = await Subprocess.call({
          command: "C:\\Windows\\System32\\tasklist.exe",
          arguments: ["/FI", `PID eq ${pid}`, "/FO", "CSV", "/NH"],
          stdout: "pipe",
          stderr: "pipe",
        });

        let output = "";
        const stdout = process.stdout;
        if (stdout) {
          while (true) {
            const chunk = await stdout.readString();
            if (chunk === null) break;
            output += chunk;
          }
        }
        await process.wait();

        return output.toLowerCase().includes(expectedName.toLowerCase());
      } else {
        // Unix: ps -p <pid> -o comm=
        const process = await Subprocess.call({
          command: "ps",
          arguments: ["-p", String(pid), "-o", "comm="],
          stdout: "pipe",
          stderr: "pipe",
        });

        let output = "";
        const stdout = process.stdout;
        if (stdout) {
          while (true) {
            const chunk = await stdout.readString();
            if (chunk === null) break;
            output += chunk;
          }
        }
        await process.wait();

        const processName = output.trim();
        return (
          processName.length > 0 &&
          (expectedName.includes(processName) ||
            processName.includes(expectedName))
        );
      }
    } catch (e) {
      console.error(`[Floorp OS] Failed to verify process name for ${pid}:`, e);
      return false;
    }
  }

  private async readRuntimeState(): Promise<RuntimeState | null> {
    const statePath = this.getRuntimeStatePath();
    try {
      if (!(await IOUtils.exists(statePath))) {
        return null;
      }
      const content = await IOUtils.readUTF8(statePath);
      const parsed = JSON.parse(content) as Partial<RuntimeState>;
      const state: RuntimeState = {
        corePid:
          typeof parsed.corePid === "number" && Number.isFinite(parsed.corePid)
            ? parsed.corePid
            : null,
        frontendPid:
          typeof parsed.frontendPid === "number" &&
          Number.isFinite(parsed.frontendPid)
            ? parsed.frontendPid
            : null,
        timestamp:
          typeof parsed.timestamp === "number" &&
          Number.isFinite(parsed.timestamp)
            ? parsed.timestamp
            : Date.now(),
      };
      this._runtimeState = state;
      return state;
    } catch (error) {
      console.error("[Floorp OS] Failed to read runtime state:", error);
      return null;
    }
  }

  private async writeRuntimeState(state: RuntimeState): Promise<void> {
    const floorpOSDir = this.getFloorpOSDirectory();
    try {
      if (!(await IOUtils.exists(floorpOSDir))) {
        await IOUtils.makeDirectory(floorpOSDir);
      }
    } catch (error) {
      console.error("[Floorp OS] Failed to ensure floorp-os directory:", error);
      return;
    }
    const statePath = this.getRuntimeStatePath();
    const encoder = new TextEncoder();
    const content = encoder.encode(JSON.stringify(state));
    try {
      await IOUtils.write(statePath, content, { mode: "overwrite" });
    } catch (error) {
      console.error("[Floorp OS] Failed to write runtime state:", error);
    }
  }

  private async updateRuntimeState(
    partial: Partial<RuntimeState>,
  ): Promise<void> {
    const current: RuntimeState = this._runtimeState ??
      (await this.readRuntimeState()) ?? {
        corePid: null,
        frontendPid: null,
        timestamp: Date.now(),
      };
    const next: RuntimeState = {
      corePid:
        partial.corePid !== undefined ? partial.corePid : current.corePid,
      frontendPid:
        partial.frontendPid !== undefined
          ? partial.frontendPid
          : current.frontendPid,
      timestamp: Date.now(),
    };
    this._runtimeState = next;
    await this.writeRuntimeState(next);
  }

  private async clearRuntimeState(): Promise<void> {
    const statePath = this.getRuntimeStatePath();
    try {
      await IOUtils.remove(statePath, { ignoreAbsent: true });
      this._runtimeState = null;
    } catch (error) {
      console.error("[Floorp OS] Failed to clear runtime state file:", error);
    }
  }
}

/**
 * Singleton instance of OSAutomotorManager.
 * Importing this module will automatically initialize the manager.
 */
export const osAutomotorManager = new OSAutomotorManager();
