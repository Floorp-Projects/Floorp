import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

const PROCESS_TYPE_DEFAULT = Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
const OS_INTEGRATION_MESSAGE = "floorp:pwa:linux-os-integration";
const DOCUMENT_READY_RETRY_LIMIT = 20;
const DOCUMENT_READY_RETRY_DELAY_MS = 100;
const GTK_READY_DELAY_MS = 300;
const ICON_RETRY_LIMIT = 10;
const ICON_RETRY_DELAY_MS = 300;
const NS_IFILE_IID = Ci.nsIFile as unknown as nsIID;

type nsIFloorpLinuxTaskbar = {
  setWindowClass(
    window: mozIDOMWindowProxy,
    windowClass: string,
    windowTitle: string,
  ): void;
  setWindowIconFromPath(window: mozIDOMWindowProxy, iconPath: string): void;
};

type LinuxPathInfo = {
  desktopBasename: string;
  desktopDir: string;
  desktopPath: string;
  iconDir: string;
  iconPath: string;
  startupWMClass: string;
};

const textEncoder = new TextEncoder();

export class LinuxSupport {
  private static nsIFile = Components.Constructor(
    "@mozilla.org/file/local;1",
    Ci.nsIFile,
    "initWithPath",
  );

  private static linuxTaskbarInstance?: nsIFloorpLinuxTaskbar | null;
  private static cachedHomeDir: string | null = null;

  private static get linuxTaskbar(): nsIFloorpLinuxTaskbar | null {
    if (this.linuxTaskbarInstance !== undefined) {
      console.debug("[LinuxSupport] Using cached Linux taskbar instance");
      return this.linuxTaskbarInstance;
    }

    console.debug("[LinuxSupport] Loading Linux taskbar integration");
    try {
      const ciWithFloorp = Ci as typeof Ci & {
        nsIFloorpLinuxTaskbar: nsIID;
      };
      const taskbarService = Cc["@noraneko.org/pwa/linux-taskbar;1"].getService(
        ciWithFloorp.nsIFloorpLinuxTaskbar,
      ) as nsIFloorpLinuxTaskbar;
      this.linuxTaskbarInstance = taskbarService;
      console.debug(
        "[LinuxSupport] Successfully loaded Linux taskbar integration",
      );
    } catch (error) {
      console.error("Failed to load Linux taskbar integration", error);
      this.linuxTaskbarInstance = null;
    }

    return this.linuxTaskbarInstance;
  }

  private static get homeDir(): string {
    if (!this.cachedHomeDir) {
      const homeFile = Services.dirsvc.get("Home", NS_IFILE_IID) as nsIFile;
      this.cachedHomeDir = homeFile.path;
      console.debug(
        `[LinuxSupport] Cached home directory: ${this.cachedHomeDir}`,
      );
    }
    // Ensure that cachedHomeDir is string (not null)
    return this.cachedHomeDir ?? "";
  }

  private static normalizeToken(raw: string) {
    return raw
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "")
      .slice(0, 64);
  }

  private static getNormalizedId(ssb: Manifest): string {
    const candidates = [ssb.id, ssb.name, ssb.start_url];
    console.debug(
      `[LinuxSupport] Getting normalized ID for SSB: ${ssb.name} (id: ${ssb.id})`,
    );
    for (const candidate of candidates) {
      if (candidate) {
        const normalized = this.normalizeToken(candidate);
        if (normalized) {
          console.debug(
            `[LinuxSupport] Normalized ID: ${normalized} (from: ${candidate})`,
          );
          return normalized;
        }
      }
    }

    const fallbackId = Services.uuid
      .generateUUID()
      .toString()
      .replace(/[{}-]/g, "")
      .slice(0, 12);
    console.debug(`[LinuxSupport] Generated fallback ID: ${fallbackId}`);
    return fallbackId;
  }

  private static getPathInfo(ssb: Manifest): LinuxPathInfo {
    const slug = this.getNormalizedId(ssb);
    const desktopBasename = `floorp-${slug}`;

    const iconDir = PathUtils.join(
      this.homeDir,
      ".local",
      "share",
      "icons",
      "hicolor",
      "128x128",
      "apps",
    );
    const iconPath = PathUtils.join(iconDir, `${desktopBasename}.png`);

    const desktopDir = PathUtils.join(
      this.homeDir,
      ".local",
      "share",
      "applications",
    );
    const desktopPath = PathUtils.join(
      desktopDir,
      `${desktopBasename}.desktop`,
    );

    const pathInfo = {
      desktopBasename,
      desktopDir,
      desktopPath,
      iconDir,
      iconPath,
      startupWMClass: desktopBasename,
    };
    console.debug(`[LinuxSupport] Path info for ${ssb.name}:`, pathInfo);
    return pathInfo;
  }

  private static async ensureDirectories(paths: LinuxPathInfo) {
    console.debug(
      `[LinuxSupport] Ensuring directories exist: iconDir=${paths.iconDir}, desktopDir=${paths.desktopDir}`,
    );
    await IOUtils.makeDirectory(paths.iconDir, {
      createAncestors: true,
      ignoreExisting: true,
    });
    await IOUtils.makeDirectory(paths.desktopDir, {
      createAncestors: true,
      ignoreExisting: true,
    });
    console.debug("[LinuxSupport] Directories ensured");
  }

  private static escapeExecToken(token: string): string {
    if (/^[\w@%+=:,./-]+$/.test(token)) {
      return token;
    }

    return `"${token.replace(/(["\\$`])/g, "\\$1")}"`;
  }

  private static buildExecCommand(ssb: Manifest): string {
    const tokens: string[] = [];

    const isFlatpak = FileUtils.File("/.flatpak-info").exists();
    console.debug(
      `[LinuxSupport] Building exec command for SSB ${ssb.id}, flatpak=${isFlatpak}`,
    );
    if (isFlatpak) {
      tokens.push("flatpak", "run", "one.ablaze.floorp");
    } else {
      const exePath = Services.dirsvc.get("XREExeF", NS_IFILE_IID).path;
      tokens.push(exePath);
      console.debug(`[LinuxSupport] Using executable path: ${exePath}`);
    }

    tokens.push("--profile", PathUtils.profileDir, "--start-ssb", ssb.id);
    const execCommand = tokens
      .map((token) => this.escapeExecToken(token))
      .join(" ");
    console.debug(`[LinuxSupport] Built exec command: ${execCommand}`);
    return execCommand;
  }

  private static buildDesktopEntry(
    ssb: Manifest,
    execCommand: string,
    iconPath: string | null,
    startupWMClass: string,
  ) {
    const lines = [
      "[Desktop Entry]",
      "Version=1.0",
      "Type=Application",
      `Name=${ssb.name}`,
      `Comment=${ssb.short_name ?? ssb.name}`,
      `Exec=${execCommand}`,
      `Icon=${iconPath ?? "floorp"}`,
      "Terminal=false",
      "Categories=Network;WebBrowser;",
      "StartupNotify=true",
      `StartupWMClass=${startupWMClass}`,
      "X-MultipleArgs=false",
      `X-Floorp-StartUrl=${ssb.start_url}`,
      `X-Floorp-Id=${ssb.id}`,
    ];

    return `${lines.join("\n")}\n`;
  }

  private static async ensureDesktopPermissions(path: string) {
    console.debug(
      `[LinuxSupport] Setting permissions for desktop entry: ${path}`,
    );
    try {
      await IOUtils.setPermissions(path, 0o755);
      console.debug(
        "[LinuxSupport] Successfully set desktop entry permissions",
      );
    } catch (error) {
      console.error("Failed to set permissions for desktop entry", error);
    }
  }

  async install(ssb: Manifest) {
    console.debug(`[LinuxSupport] Installing SSB: ${ssb.name} (id: ${ssb.id})`);
    const paths = LinuxSupport.getPathInfo(ssb);
    await LinuxSupport.ensureDirectories(paths);

    let iconFile: nsIFile | null = new LinuxSupport.nsIFile(paths.iconPath);
    if (ssb.icon) {
      console.debug(`[LinuxSupport] Loading icon from: ${ssb.icon}`);
      try {
        const { container } = await ImageTools.loadImage(
          Services.io.newURI(ssb.icon),
        );
        console.debug(`[LinuxSupport] Saving icon to: ${paths.iconPath}`);
        await ImageTools.saveIcon(container, 128, 128, iconFile);
        console.debug("[LinuxSupport] Icon saved successfully");
      } catch (error) {
        console.error("Failed to save SSB icon for Linux", error);
        iconFile = null;
      }
    } else {
      console.debug("[LinuxSupport] No icon specified for SSB");
      iconFile = null;
    }

    const execCommand = LinuxSupport.buildExecCommand(ssb);
    const desktopEntry = LinuxSupport.buildDesktopEntry(
      ssb,
      execCommand,
      iconFile?.path ?? null,
      paths.startupWMClass,
    );

    console.debug(
      `[LinuxSupport] Writing desktop entry to: ${paths.desktopPath}`,
    );
    await IOUtils.write(paths.desktopPath, textEncoder.encode(desktopEntry));
    await LinuxSupport.ensureDesktopPermissions(paths.desktopPath);
    console.debug(`[LinuxSupport] Successfully installed SSB: ${ssb.name}`);
  }

  async uninstall(ssb: Manifest) {
    console.debug(
      `[LinuxSupport] Uninstalling SSB: ${ssb.name} (id: ${ssb.id})`,
    );
    const paths = LinuxSupport.getPathInfo(ssb);

    console.debug(
      `[LinuxSupport] Removing desktop entry: ${paths.desktopPath}`,
    );
    try {
      await IOUtils.remove(paths.desktopPath, { ignoreAbsent: true });
      console.debug("[LinuxSupport] Desktop entry removed successfully");
    } catch (e) {
      console.error("[LinuxSupport] Failed to remove desktop entry", e);
    }

    console.debug(`[LinuxSupport] Removing icon: ${paths.iconPath}`);
    try {
      await IOUtils.remove(paths.iconPath, {
        recursive: true,
        ignoreAbsent: true,
      });
      console.debug("[LinuxSupport] Icon removed successfully");
    } catch (e) {
      console.error("[LinuxSupport] Failed to remove icon", e);
    }
    console.debug(`[LinuxSupport] Successfully uninstalled SSB: ${ssb.name}`);
  }

  async applyOSIntegration(ssb: Manifest, aWindow: Window) {
    if (Services.appinfo.processType !== PROCESS_TYPE_DEFAULT) {
      console.debug(
        "[LinuxSupport] Forwarding OS integration request to parent process",
      );
      LinuxSupport.sendParentProcessRequest({
        action: "apply",
        ssb,
        windowName: aWindow?.name ?? null,
      });
      return;
    }

    await LinuxSupport.applyOSIntegrationInternal(ssb, aWindow);
  }

  private static async applyOSIntegrationInternal(
    ssb: Manifest,
    aWindow: Window,
  ) {
    console.debug(
      `[LinuxSupport] Applying OS integration for SSB: ${ssb.name} (id: ${ssb.id})`,
    );
    const taskbar = LinuxSupport.linuxTaskbar;
    if (!taskbar) {
      console.debug(
        "[LinuxSupport] Linux taskbar integration not available, skipping OS integration",
      );
      return;
    }

    const paths = LinuxSupport.getPathInfo(ssb);
    try {
      const documentReady = await LinuxSupport.waitForDocumentReady(aWindow);
      if (!documentReady) {
        console.warn(
          "[LinuxSupport] Window document not ready after max retries, proceeding anyway",
        );
      }

      await LinuxSupport.sleep(GTK_READY_DELAY_MS);

      console.debug(
        `[LinuxSupport] Setting window class: ${paths.startupWMClass}, title: ${ssb.name}`,
      );
      LinuxSupport.trySetWindowClass(taskbar, aWindow, paths, ssb);

      const iconExists = await IOUtils.exists(paths.iconPath);
      console.debug(
        `[LinuxSupport] Icon exists: ${iconExists} at ${paths.iconPath}`,
      );
      if (iconExists) {
        await LinuxSupport.trySetWindowIcon(taskbar, aWindow, paths);
      } else {
        console.warn(
          `[LinuxSupport] Icon file does not exist: ${paths.iconPath}`,
        );
      }
      console.debug("[LinuxSupport] OS integration applied successfully");
    } catch (error) {
      console.error("Failed to apply Linux OS integration", error);
    }
  }

  private static sendParentProcessRequest(data: {
    action: "apply";
    ssb: Manifest;
    windowName: string | null;
  }) {
    if (!("cpmm" in Services) || !Services.cpmm) {
      console.error(
        "[LinuxSupport] Parent process messaging not available; cannot apply OS integration",
      );
      return;
    }
    Services.cpmm.sendAsyncMessage(OS_INTEGRATION_MESSAGE, data);
  }

  private static parentListenerRegistered = false;

  static init() {
    if (
      Services.appinfo.processType !== PROCESS_TYPE_DEFAULT ||
      LinuxSupport.parentListenerRegistered ||
      !("ppmm" in Services) ||
      !Services.ppmm
    ) {
      return;
    }

    Services.ppmm.addMessageListener(OS_INTEGRATION_MESSAGE, {
      receiveMessage: (message) => LinuxSupport.handleParentMessage(message),
    });
    LinuxSupport.parentListenerRegistered = true;
  }

  private static handleParentMessage = async (message: {
    data?: { action?: string; ssb?: Manifest; windowName?: string | null };
  }) => {
    const { data } = message;
    if (!data || data.action !== "apply" || !data.ssb) {
      return;
    }

    const targetWindow = await LinuxSupport.waitForWindowByName(
      data.windowName ?? null,
    );
    if (!targetWindow) {
      console.warn(
        `[LinuxSupport] Could not locate window '${data.windowName}' for OS integration`,
      );
      return;
    }

    await LinuxSupport.applyOSIntegrationInternal(data.ssb, targetWindow);
  };

  private static async waitForWindowByName(
    windowName: string | null,
    timeoutMs: number = 5000,
  ): Promise<Window | null> {
    if (!windowName) {
      return null;
    }
    const resolvedName: string = windowName;
    const start = Date.now();
    while (Date.now() - start < timeoutMs) {
      const win = LinuxSupport.getWindowByName(resolvedName);
      if (win) {
        return win;
      }
      await LinuxSupport.sleep(DOCUMENT_READY_RETRY_DELAY_MS);
    }
    return null;
  }

  private static getWindowByName(windowName: string): Window | null {
    const enumerator = Services.wm.getEnumerator("");
    while (enumerator.hasMoreElements()) {
      const win = enumerator.getNext() as Window;
      if (win?.name === windowName) {
        return win;
      }
    }
    return null;
  }

  private static async waitForDocumentReady(
    aWindow: Window,
    maxRetries: number = DOCUMENT_READY_RETRY_LIMIT,
  ): Promise<boolean> {
    let retries = 0;
    while (retries < maxRetries) {
      if (aWindow.document && aWindow.document.readyState === "complete") {
        console.debug(
          `[LinuxSupport] Window document ready after ${retries} retries`,
        );
        return true;
      }
      await LinuxSupport.sleep(DOCUMENT_READY_RETRY_DELAY_MS);
      retries++;
    }
    return false;
  }

  private static trySetWindowClass(
    taskbar: nsIFloorpLinuxTaskbar,
    aWindow: Window,
    paths: LinuxPathInfo,
    ssb: Manifest,
  ) {
    try {
      taskbar.setWindowClass(
        aWindow as unknown as mozIDOMWindowProxy,
        paths.startupWMClass,
        ssb.name,
      );
      console.debug("[LinuxSupport] Window class set successfully");
    } catch (error) {
      console.error("[LinuxSupport] Failed to set window class", error);
    }
  }

  private static async trySetWindowIcon(
    taskbar: nsIFloorpLinuxTaskbar,
    aWindow: Window,
    paths: LinuxPathInfo,
  ) {
    let iconRetries = 0;
    while (iconRetries < ICON_RETRY_LIMIT) {
      try {
        console.debug(
          `[LinuxSupport] Setting window icon from path (attempt ${iconRetries + 1}/${ICON_RETRY_LIMIT}): ${paths.iconPath}`,
        );
        taskbar.setWindowIconFromPath(
          aWindow as unknown as mozIDOMWindowProxy,
          paths.iconPath,
        );
        console.debug("[LinuxSupport] Window icon set successfully");
        return;
      } catch (error) {
        iconRetries++;
        if (iconRetries >= ICON_RETRY_LIMIT) {
          console.error(
            "[LinuxSupport] Failed to set window icon after retries",
            error,
          );
          return;
        }
        console.debug(
          `[LinuxSupport] Icon set failed, retrying in ${ICON_RETRY_DELAY_MS}ms... (${error})`,
        );
        await LinuxSupport.sleep(ICON_RETRY_DELAY_MS);
      }
    }
  }

  private static sleep(ms: number) {
    return new Promise<void>((resolve) => setTimeout(resolve, ms));
  }
}

LinuxSupport.init();
