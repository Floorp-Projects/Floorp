/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * External-Browser - A module for detecting installed browsers and opening URLs in them
 *
 * This module provides functionality to:
 * - Detect installed browsers on Windows, macOS, and Linux
 * - Open URLs in external browsers from context menu actions
 */

import type {
  BrowserDetectionConfig,
  ExternalBrowser,
  OpenInBrowserOptions,
  OpenResult,
} from "./type.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

interface SubprocessCallOptions {
  command: string;
  arguments?: string[];
  stdout?: "pipe";
  stderr?: "pipe";
}

interface SubprocessProcess {
  stdout?: { readString(): Promise<string | null> };
  stderr?: { readString(): Promise<string | null> };
  wait(): Promise<{ exitCode: number }>;
}

interface SubprocessModule {
  call(options: SubprocessCallOptions): Promise<SubprocessProcess>;
}

const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs",
) as { Subprocess: SubprocessModule };

/**
 * Configuration for browsers we want to detect
 */
const BROWSER_CONFIGS: BrowserDetectionConfig[] = [
  {
    id: "chrome",
    name: "Google Chrome",
    icon: "chrome",
    windowsPaths: [
      "${LOCALAPPDATA}\\Google\\Chrome\\Application\\chrome.exe",
      "${PROGRAMFILES}\\Google\\Chrome\\Application\\chrome.exe",
      "${PROGRAMFILES(X86)}\\Google\\Chrome\\Application\\chrome.exe",
    ],
    macPaths: ["/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"],
    macBundleId: "com.google.Chrome",
    linuxPaths: [
      "/usr/bin/google-chrome",
      "/usr/bin/google-chrome-stable",
      "/usr/local/bin/google-chrome",
      "/snap/bin/chromium",
      "/usr/bin/chromium",
      "/usr/bin/chromium-browser",
    ],
  },
  {
    id: "edge",
    name: "Microsoft Edge",
    icon: "edge",
    windowsPaths: [
      "${PROGRAMFILES}\\Microsoft\\Edge\\Application\\msedge.exe",
      "${PROGRAMFILES(X86)}\\Microsoft\\Edge\\Application\\msedge.exe",
      "${LOCALAPPDATA}\\Microsoft\\Edge\\Application\\msedge.exe",
    ],
    macPaths: [
      "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
    ],
    macBundleId: "com.microsoft.edgemac",
    linuxPaths: [
      "/usr/bin/microsoft-edge",
      "/usr/bin/microsoft-edge-stable",
      "/opt/microsoft/msedge/msedge",
    ],
  },
  {
    id: "safari",
    name: "Safari",
    icon: "safari",
    // Safari is macOS only
    macPaths: ["/Applications/Safari.app/Contents/MacOS/Safari"],
    macBundleId: "com.apple.Safari",
  },
  {
    id: "brave",
    name: "Brave",
    icon: "brave",
    windowsPaths: [
      "${LOCALAPPDATA}\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
      "${PROGRAMFILES}\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
      "${PROGRAMFILES(X86)}\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
    ],
    macPaths: ["/Applications/Brave Browser.app/Contents/MacOS/Brave Browser"],
    macBundleId: "com.brave.Browser",
    linuxPaths: ["/usr/bin/brave-browser", "/usr/bin/brave", "/snap/bin/brave"],
  },
  {
    id: "vivaldi",
    name: "Vivaldi",
    icon: "vivaldi",
    windowsPaths: [
      "${LOCALAPPDATA}\\Vivaldi\\Application\\vivaldi.exe",
      "${PROGRAMFILES}\\Vivaldi\\Application\\vivaldi.exe",
    ],
    macPaths: ["/Applications/Vivaldi.app/Contents/MacOS/Vivaldi"],
    macBundleId: "com.vivaldi.Vivaldi",
    linuxPaths: ["/usr/bin/vivaldi", "/usr/bin/vivaldi-stable"],
  },
  {
    id: "opera",
    name: "Opera",
    icon: "opera",
    windowsPaths: [
      "${LOCALAPPDATA}\\Programs\\Opera\\launcher.exe",
      "${PROGRAMFILES}\\Opera\\launcher.exe",
    ],
    macPaths: ["/Applications/Opera.app/Contents/MacOS/Opera"],
    macBundleId: "com.operasoftware.Opera",
    linuxPaths: ["/usr/bin/opera", "/snap/bin/opera"],
  },
  {
    id: "firefox",
    name: "Mozilla Firefox",
    icon: "firefox",
    windowsPaths: [
      "${PROGRAMFILES}\\Mozilla Firefox\\firefox.exe",
      "${PROGRAMFILES(X86)}\\Mozilla Firefox\\firefox.exe",
    ],
    macPaths: ["/Applications/Firefox.app/Contents/MacOS/firefox"],
    macBundleId: "org.mozilla.firefox",
    linuxPaths: [
      "/usr/bin/firefox",
      "/usr/lib/firefox/firefox",
      "/snap/bin/firefox",
    ],
  },
  {
    id: "arc",
    name: "Arc",
    icon: "arc",
    // Arc is macOS only (Windows version in development)
    macPaths: ["/Applications/Arc.app/Contents/MacOS/Arc"],
    macBundleId: "company.thebrowser.Browser",
    windowsPaths: ["${LOCALAPPDATA}\\Arc\\Application\\Arc.exe"],
  },
  {
    id: "zen",
    name: "Zen Browser",
    icon: "zen",
    windowsPaths: [
      "${PROGRAMFILES}\\Zen Browser\\zen.exe",
      "${LOCALAPPDATA}\\Zen Browser\\zen.exe",
    ],
    macPaths: ["/Applications/Zen Browser.app/Contents/MacOS/zen"],
    macBundleId: "app.zen.browser",
    linuxPaths: ["/usr/bin/zen-browser", "/opt/zen-browser/zen"],
  },
];

/**
 * Manager class for external browser detection and operations
 */
class ExternalBrowserManager {
  private _cachedBrowsers: ExternalBrowser[] | null = null;
  private _cacheTimestamp: number = 0;
  private readonly CACHE_TTL_MS = 60000; // Cache for 1 minute

  /**
   * Get the current platform
   */
  private get platform(): string {
    return AppConstants.platform;
  }

  /**
   * Expand environment variables in a path (Windows)
   */
  private expandWindowsPath(path: string): string {
    const envVars: Record<string, string | undefined> = {
      LOCALAPPDATA: Services.env.get("LOCALAPPDATA"),
      PROGRAMFILES: Services.env.get("ProgramFiles"),
      "PROGRAMFILES(X86)": Services.env.get("ProgramFiles(x86)"),
      APPDATA: Services.env.get("APPDATA"),
      USERPROFILE: Services.env.get("USERPROFILE"),
    };

    let expandedPath = path;
    for (const [varName, value] of Object.entries(envVars)) {
      if (value) {
        expandedPath = expandedPath.replace(`\${${varName}}`, value);
      }
    }
    return expandedPath;
  }

  /**
   * Check if a file exists
   */
  private async fileExists(path: string): Promise<boolean> {
    try {
      return await IOUtils.exists(path);
    } catch {
      return false;
    }
  }

  /**
   * Try to find browser path on macOS using mdfind
   */
  private async findMacAppByBundleId(bundleId: string): Promise<string | null> {
    try {
      const process = await Subprocess.call({
        command: "/usr/bin/mdfind",
        arguments: [`kMDItemCFBundleIdentifier == "${bundleId}"`],
        stdout: "pipe",
        stderr: "pipe",
      });

      const output = await process.stdout?.readString();
      await process.wait();

      if (output) {
        const paths = output
          .trim()
          .split("\n")
          .filter((p) => p.length > 0);
        if (paths.length > 0) {
          // Return the first found application path
          const appPath = paths[0];
          // Construct the executable path
          const execPath = `${appPath}/Contents/MacOS/${appPath.split("/").pop()?.replace(".app", "")}`;
          if (await this.fileExists(execPath)) {
            return execPath;
          }
        }
      }
    } catch (error) {
      console.debug(
        `[ExternalBrowser] Failed to find app by bundle ID ${bundleId}:`,
        error,
      );
    }
    return null;
  }

  /**
   * Detect a single browser based on configuration
   */
  private async detectBrowser(
    config: BrowserDetectionConfig,
  ): Promise<ExternalBrowser | null> {
    let paths: string[] = [];
    let foundPath: string | null = null;

    switch (this.platform) {
      case "win":
        paths = (config.windowsPaths || []).map((p) =>
          this.expandWindowsPath(p),
        );
        break;
      case "macosx":
        paths = config.macPaths || [];
        break;
      case "linux":
        paths = config.linuxPaths || [];
        break;
      default:
        return null;
    }

    // Check each path
    for (const path of paths) {
      if (await this.fileExists(path)) {
        foundPath = path;
        break;
      }
    }

    // On macOS, try mdfind if no path found and bundleId is available
    if (!foundPath && this.platform === "macosx" && config.macBundleId) {
      foundPath = await this.findMacAppByBundleId(config.macBundleId);
    }

    if (foundPath) {
      return {
        id: config.id,
        name: config.name,
        path: foundPath,
        icon: config.icon,
        available: true,
      };
    }

    return null;
  }

  /**
   * Detect all installed browsers on the system
   */
  public async detectInstalledBrowsers(
    forceRefresh = false,
  ): Promise<ExternalBrowser[]> {
    // Check cache validity
    const now = Date.now();
    if (
      !forceRefresh &&
      this._cachedBrowsers &&
      now - this._cacheTimestamp < this.CACHE_TTL_MS
    ) {
      return this._cachedBrowsers;
    }

    console.info("[ExternalBrowser] Detecting installed browsers...");
    const browsers: ExternalBrowser[] = [];

    // Get Floorp's own path to exclude it
    const floorpPath = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;

    for (const config of BROWSER_CONFIGS) {
      try {
        const browser = await this.detectBrowser(config);
        if (browser) {
          // Exclude Floorp itself (compare normalized paths)
          const normalizedBrowserPath = browser.path.toLowerCase();
          const normalizedFloorpPath = floorpPath.toLowerCase();
          if (normalizedBrowserPath !== normalizedFloorpPath) {
            browsers.push(browser);
            console.debug(
              `[ExternalBrowser] Found ${browser.name} at ${browser.path}`,
            );
          }
        }
      } catch (error) {
        console.error(
          `[ExternalBrowser] Error detecting ${config.name}:`,
          error,
        );
      }
    }

    // Update cache
    this._cachedBrowsers = browsers;
    this._cacheTimestamp = now;

    console.info(`[ExternalBrowser] Detected ${browsers.length} browsers`);
    return browsers;
  }

  /**
   * Get a specific browser by ID
   */
  public async getBrowserById(id: string): Promise<ExternalBrowser | null> {
    const browsers = await this.detectInstalledBrowsers();
    return browsers.find((b) => b.id === id) || null;
  }

  /**
   * Open a URL in an external browser
   */
  public async openInBrowser(
    options: OpenInBrowserOptions,
  ): Promise<OpenResult> {
    const { url, browserId, privateMode } = options;

    try {
      let browser: ExternalBrowser | null = null;

      if (browserId) {
        browser = await this.getBrowserById(browserId);
        if (!browser) {
          return {
            success: false,
            error: `Browser not found: ${browserId}`,
          };
        }
      } else {
        // Use default system browser via nsIExternalProtocolService
        return this.openWithSystemDefault(url);
      }

      // Safari on macOS requires special handling via the 'open' command
      if (browser.id === "safari" && this.platform === "macosx") {
        return this.openWithMacOpen(url, "Safari", privateMode);
      }

      // Build command arguments
      const args: string[] = [];

      // Add private/incognito flags based on browser
      if (privateMode) {
        switch (browser.id) {
          case "chrome":
          case "edge":
          case "brave":
          case "vivaldi":
          case "arc":
            args.push("--incognito");
            break;
          case "firefox":
          case "zen":
            args.push("--private-window");
            break;
          case "opera":
            args.push("--private");
            break;
        }
      }

      // Add the URL
      args.push(url);

      console.info(`[ExternalBrowser] Opening ${url} in ${browser.name}`, args);

      // Launch the browser
      const process = await Subprocess.call({
        command: browser.path,
        arguments: args,
      });

      // Don't wait for the process to complete (it's a browser, it will keep running)
      void process.wait().catch((error: unknown) => {
        console.error(`[ExternalBrowser] Process error:`, error);
      });

      return { success: true };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[ExternalBrowser] Failed to open URL:`, error);
      return {
        success: false,
        error: errorMessage,
      };
    }
  }

  /**
   * Open URL using macOS 'open' command with a specific application
   * This is required for Safari which doesn't accept URLs as command-line arguments
   */
  private async openWithMacOpen(
    url: string,
    appName: string,
    _privateMode = false,
  ): Promise<OpenResult> {
    try {
      // Safari doesn't support private mode via command line
      // For other apps that need 'open', we could add --args flags

      const args = ["-a", appName, url];

      console.info(
        `[ExternalBrowser] Opening ${url} with macOS open command:`,
        args,
      );

      const process = await Subprocess.call({
        command: "/usr/bin/open",
        arguments: args,
      });

      void process.wait().catch((error: unknown) => {
        console.error(`[ExternalBrowser] Process error:`, error);
      });

      return { success: true };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[ExternalBrowser] Failed to open with macOS open:`, error);
      return {
        success: false,
        error: errorMessage,
      };
    }
  }

  /**
   * Open URL using the system default handler
   */
  private openWithSystemDefault(url: string): OpenResult {
    try {
      const externalProtocolService = Cc[
        "@mozilla.org/uriloader/external-protocol-service;1"
      ].getService(Ci.nsIExternalProtocolService);

      const uri = Services.io.newURI(url);
      externalProtocolService.loadURI(uri);

      return { success: true };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[ExternalBrowser] Failed to open with system default:`,
        error,
      );
      return {
        success: false,
        error: errorMessage,
      };
    }
  }

  /**
   * Get the system's default browser information (if detectable)
   */
  public getDefaultBrowser(): ExternalBrowser | null {
    // This is platform-specific and complex to implement fully
    // For now, we return null and rely on openWithSystemDefault for default behavior
    // Future enhancement: Read default browser from registry (Windows) or
    // defaults read (macOS) or xdg-settings (Linux)
    return null;
  }

  /**
   * Clear the browser cache (useful after browser installation/uninstallation)
   */
  public clearCache(): void {
    this._cachedBrowsers = null;
    this._cacheTimestamp = 0;
  }
}

/**
 * Singleton instance of ExternalBrowserManager
 */
export const externalBrowserManager = new ExternalBrowserManager();

/**
 * Convenience export for common operations
 */
export const ExternalBrowserService = {
  /**
   * Get all detected external browsers
   */
  getInstalledBrowsers: (forceRefresh = false) =>
    externalBrowserManager.detectInstalledBrowsers(forceRefresh),

  /**
   * Open a URL in a specific browser
   */
  openUrl: (url: string, browserId?: string, privateMode = false) =>
    externalBrowserManager.openInBrowser({ url, browserId, privateMode }),

  /**
   * Open a URL in the system default browser
   */
  openInDefaultBrowser: (url: string) =>
    externalBrowserManager.openInBrowser({ url }),

  /**
   * Get a browser by its ID
   */
  getBrowser: (id: string) => externalBrowserManager.getBrowserById(id),

  /**
   * Clear the browser detection cache
   */
  clearCache: () => externalBrowserManager.clearCache(),
};
