interface FirefoxWindow extends Window {
  gBrowser: {
    selectedTab: unknown;
    addTab(url: string, options: unknown): unknown;
  };
}

interface XULElement extends Element {}

// In Firefox browser chrome context, document is always available.
// Gecko types declare it as `Document | null`, but it is never null here.
const doc = document!;

const FLOORP_HUB_WARNING_IDS = {
  container: "floorp-hub-warning",
  message: "floorp-hub-warning-message",
  button: "floorp-hub-warning-button",
  style: "floorp-hub-warning-style",
} as const;

type FloorpHubWarningTexts = {
  message: string;
  buttonLabel: string;
};

type I18NextLike = {
  isInitialized: boolean;
  on(event: string, callback: () => void): void;
  t(key: string, options?: Record<string, unknown>): string | string[];
};

let i18nextInstance: I18NextLike | null = null;

function logFloorpHub(..._messages: Array<unknown>): void {
  // Silent in production: this wrapper previously emitted noisy debug
  // output via console.log. Keep as a no-op to avoid cluttering logs.
  return;
}

function getI18nextInstance(): I18NextLike | null {
  if (i18nextInstance) {
    logFloorpHub("Reusing cached i18next instance.");
    return i18nextInstance;
  }

  try {
    const module = ChromeUtils.importESModule(
      "chrome://noraneko/content/assets/js/external/i18next.js",
    ) as Record<string, unknown>;

    // Try common export shapes used across bundles
    // prefer: module.i, module.i18next, module.default?.i, module.default, module
    // Log what we find to aid debugging in runtime logs
    try {
      logFloorpHub(
        "Imported i18next module exports:",
        Object.keys(module ?? {}),
      );
    } catch {
      // ignore if module is not an object
    }

    // Try variants
    // Try variants in a type-safe way
    const m = module as Record<string, unknown> | null;
    let resolved: unknown | null = null;
    if (m) {
      if ("i" in m && m["i"]) {
        resolved = m["i"];
      } else if ("i18next" in m && m["i18next"]) {
        resolved = m["i18next"];
      } else if ("default" in m && m["default"]) {
        const def = m["default"] as Record<string, unknown>;
        if ("i" in def && def["i"]) resolved = def["i"];
        else resolved = def;
      } else if (
        "t" in m &&
        m["t"] &&
        typeof (m["t"] as { t?: unknown }).t === "function"
      ) {
        // vite v8 / rolldown bundle: `export { instance as t }`
        // module.t holds the i18next instance which has its own .t method.
        resolved = m["t"];
      } else {
        resolved = m;
      }
    }

    const maybe = resolved as {
      t?: unknown;
      isInitialized?: boolean;
      on?: unknown;
    } | null;
    if (maybe && typeof maybe.t === "function") {
      i18nextInstance = maybe as I18NextLike;
      logFloorpHub("Successfully resolved i18next instance from module.");
    } else {
      console.error(
        "Floorp Hub:",
        "Imported module did not expose a usable i18next instance.",
        module,
      );
      i18nextInstance = null;
    }
  } catch (error) {
    console.error("Floorp Hub:", "Failed to import i18next module", error);
    i18nextInstance = null;
  }

  return i18nextInstance;
}

type FloorpHubWarningElements = {
  container: XULElement;
  message: XULElement;
  button: XULElement;
};

const defaultWarningTexts: FloorpHubWarningTexts = {
  message:
    "Floorp-specific settings such as the panel sidebar, Workspaces, and mouse gestures are managed in Floorp Hub.",
  buttonLabel: "Open Floorp Hub",
};

// Note: localized fallback maps removed for preferences — preferences will
// now use default texts when translations are not available.

let floorpHubWarningElements: FloorpHubWarningElements | null = null;
let warningI18nHandlersBound = false;
let localeObserverBound = false;
let i18nUtilsInstance:
  | {
      getPrimaryBrowserLocaleMapped(): string;
      addLocaleChangeListener(callback: (locale: string) => void): void;
    }
  | null
  | undefined;

function ensureI18NInitialized(): void {
  const instance = getI18nextInstance();
  if (!instance) {
    logFloorpHub("i18next instance unavailable; using fallback text.");
    return;
  }

  // Dump diagnostics to help determine why translations may not be available
  logI18nextDiagnostics(instance);
  if (!instance.isInitialized) {
    console.warn(
      "Floorp Hub:",
      "i18next not initialized; using fallback strings.",
    );
    return;
  }

  logFloorpHub("i18next already initialized.");
  bindGlobalI18nInitializedObserver();
}

let globalI18nInitializedObserverBound = false;
const i18nInitializedObserver = {
  observe(...args: Array<unknown>): void {
    try {
      const topic = args[1] as string | undefined;
      if (topic !== "noraneko-i18n-initialized") return;
      logFloorpHub(
        "Received noraneko-i18n-initialized; applying translations.",
      );
      applyFloorpHubWarningTexts();
      applyFloorpStartWarningTexts();
    } catch (e) {
      // Prevent observer exceptions from bubbling and stopping other observers
      // Log with a clear prefix to make debugging easier in runtime logs.
      try {
        console.error(
          "Floorp Hub: error in noraneko-i18n-initialized observer",
          e,
        );
      } catch {
        // swallow any errors thrown while attempting to log
      }
    }
  },
};

function bindGlobalI18nInitializedObserver(): void {
  if (globalI18nInitializedObserverBound) return;
  try {
    Services.obs.addObserver(i18nInitializedObserver, "noraneko-i18n-initialized");
    globalI18nInitializedObserverBound = true;
    logFloorpHub("Bound global i18n initialized observer.");
    // Clean up when the preferences page is unloaded to avoid leaking the observer.
    window.addEventListener(
      "pagehide",
      () => {
        try {
          Services.obs.removeObserver(
            i18nInitializedObserver,
            "noraneko-i18n-initialized",
          );
          globalI18nInitializedObserverBound = false;
        } catch {
          // best-effort cleanup
        }
      },
      { once: true },
    );
  } catch {
    // best-effort; ignore if Services not available
  }
}

function openFloorpHub(): void {
  if (import.meta.env.MODE === "dev") {
    logFloorpHub("Opening Floorp Hub in development mode.");
    // Use globalThis in environments where `window` may not be defined.
    (globalThis as unknown as Window).location.href = "http://localhost:5183/";
    return;
  }

  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as FirefoxWindow | null;
  if (!win) {
    console.error("Floorp Hub: No browser window available to open hub tab.");
    return;
  }
  logFloorpHub("Opening Floorp Hub tab in browser window.");
  win.gBrowser.selectedTab = win.gBrowser.addTab("about:hub", {
    relatedToCurrent: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
}

/**
 * Add Floorp Hub category element.
 * Firefox 151+ uses moz-page-nav-button custom elements instead of richlistitem.
 */
function addFloorpHubCategory(): void {
  const nav_root = doc.querySelector("#categories");
  const before_fragment = doc.querySelector("#category-sync");

  if (!nav_root || !before_fragment) {
    console.error("Category elements not found");
    return;
  }

  const hubButton = doc.createElement(
    "moz-page-nav-button",
  ) as HTMLElement & { activate(): void };
  hubButton.id = "category-nora-link";
  hubButton.setAttribute(
    "iconsrc",
    "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyMCAyMCIgd2lkdGg9IjIwIiBoZWlnaHQ9IjIwIiBjbGFzcz0iY2F0ZWdvcnktaWNvbi1zdmciPgogIDxzdHlsZT4KICAgIEBtZWRpYSAocHJlZmVycy1jb2xvci1zY2hlbWU6IGRhcmspIHsKICAgICAgLmljb24tcGF0aCB7IGZpbGw6ICNmZmZmZmY7IH0KICAgIH0KICAgIEBtZWRpYSAocHJlZmVycy1jb2xvci1zY2hlbWU6IGxpZ2h0KSB7CiAgICAgIC5pY29uLXBhdGggeyBmaWxsOiAjMDAwMDAwOyB9CiAgICB9CiAgPC9zdHlsZT4KICA8cGF0aCBjbGFzcz0iaWNvbi1wYXRoIiBkPSJNNi4zMTM3OSAwLjk0Mjg3M0M1LjQ2NzUzIDAuOTQyODczIDQuNDI1OCAxLjU1Mjk0IDQuMDAyNjYgMi4yODU4M0wwLjMxNzM1MSA4LjY4ODI4Qy0wLjEwNTc4NCA5LjQyMTE3LTAuMTA1Nzg0IDEwLjU3ODggMC4zMTczNTEgMTEuMzExN0w0LjAwMjY2IDE3LjcxNDJDNC40MjU4IDE4LjQ0NzEgNS40Njc1MiAxOS4wNTcxIDYuMzEzNzkgMTkuMDU3MUwxMy42ODQ0IDE5LjA1NzFDMTQuNTMwNyAxOS4wNTcxIDE1LjU3MjQgMTguNDQ3MSAxNS45OTU1IDE3LjcxNDJMMTkuNjgwOSAxMS4zMTE3QzIwLjEwNCAxMC41Nzg4IDIwLjEwNCA5LjQyMTE3IDE5LjY4MDkgOC42ODgyOEwxNS45OTU1IDIuMjg1ODNDMTUuNTcyNCAxLjU1Mjk0IDE0LjUzMDcgMC45NDI4NzMgMTMuNjg0NCAwLjk0Mjg3M0w2LjMxMzc5IDAuOTQyODczWk02LjMxMzc5IDIuNDQxOThMMTMuNjg0NCAyLjQ0MTk4QzEzLjk5NTEgMi40NDE5OCAxNC41Mjg1IDIuNzY2MzIgMTQuNjgzOCAzLjAzNTM4TDE4LjM2OTEgOS40Mzc4M0MxOC41MjQ1IDkuNzA2ODkgMTguNTI0NSAxMC4yOTMxIDE4LjM2OTEgMTAuNTYyMkwxNC42ODM4IDE2Ljk2NDZDMTQuNTI4NSAxNy4yMzM3IDEzLjk5NTEgMTcuNTU4IDEzLjY4NDQgMTcuNTU4TDYuMzEzNzkgMTcuNTU4QzYuMDAzMSAxNy41NTggNS40Njk3MyAxNy4yMzM3IDUuMzE0MzkgMTYuOTY0NkwxLjYyOTA3IDEwLjU2MjJCMS40NzM3MyAxMC4yOTMxIDEuNDczNzMgOS43MDY4OSAxLjYyOTA3IDkuNDM3ODNMNS4zMTQzOSAzLjAzNTM4QzUuNDY5NzMgMi43NjYzMiA2LjAwMzEgMi40NDE5OCA2LjMxMzc5IDIuNDQxOThaTTkuMTI0NjMgMy41MDM4NUM4LjUyMzkyIDMuNTAzODUgOC4wMTY5NCAzLjk3MDI2IDcuOTM3ODMgNC41NjU3Mkw3Ljc4MTY3IDUuNzIxMjlMNy40MDY4OSA1LjkzOTkxTDYuMzEzNzkgNS41MDI2N0M1Ljc1ODI3IDUuMjcyNjMgNS4xMTUxIDUuNDgxOTMgNC44MTQ2OCA2LjAwMjM3TDMuOTQwMiA3LjUwMTQ4QzMuNjQwMDQgOC4wMjE0OSAzLjc3NjI4IDguNjk2NjUgNC4yNTI1MiA5LjA2MzA2TDUuMTg5NDYgOS43ODEzOEw1LjE4OTQ2IDEwLjIxODZMNC4yNTI1MiAxMC45MzY5QzMuNzc2MTggMTEuMzAzMyAzLjYzOTk3IDExLjk3ODQgMy45NDAyIDEyLjQ5ODVMNC44MTQ2OCAxMy45OTc2QzUuMTE1MDUgMTQuNTE4IDUuNzU4NDggMTQuNzI2NiA2LjMxMzc5IDE0LjQ5NzNMNy40MDY4OSAxNC4wNjAxTDcuNzgxNjcgMTQuMjc4N0w3LjkzNzgzIDE1LjQzNDNDOC4wMTY5NSAxNi4wMjk4IDguNTIzOTIgMTYuNDk2MSA5LjEyNDYzIDE2LjQ5NjFMMTAuODczNiAxNi40OTYxQzExLjQ3NDMgMTYuNDk2MSAxMS45ODEzIDE2LjAyOTcgMTIuMDYwNCAxNS40MzQzTDEyLjIxNjUgMTQuMjc4N0wxMi41OTEzIDE0LjA2MDFMMTMuNjg0NCAxNC40OTczQzE0LjIzOTggMTQuNzI2NiAxNC44ODMyIDE0LjUxNzkgMTUuMTgzNSAxMy45OTc2TDE2LjA1OCAxMi40OTg1QzE2LjM1ODQgMTEuOTc4MSAxNi4yMjI2IDExLjMwMjkgMTUuNzQ1NyAxMC45MzY5TDE0LjgwODggMTAuMjE4NkwxNC44MDg4IDkuNzgxMzhMMTUuNzQ1NyA5LjA2MzA2QzE2LjIyMjEgOC42OTY2MyAxNi4zNTgyIDguMDIxNTYgMTYuMDU4IDcuNTAxNDhMMTUuMTgzNSA2LjAwMjM3QzE0Ljg4MzIgNS40ODIgMTQuMjM5NyA1LjI3MzM1IDEzLjY4NDQgNS41MDI2N0wxMi41OTEzIDUuOTM5OTFMMTIuMjE2NSA1LjcyMTI5TDEyLjA2MDQgNC41NjU3MkMxMS45ODEzIDMuOTcwMjEgMTEuNDc0MyAzLjUwMzg1IDEwLjg3MzYgMy41MDM4NUMxMC42NTcyIDMuNTAzODUgOS4zNDA5NyAzLjUwMzg1IDkuMTI0NjMgMy41MDM4NVpNOS4zNzQ0OCA1LjAwMjk2QzkuNzE4MjQgNS4wMDI5NiAxMC4yOCA1LjAwMjk2IDEwLjYyMzcgNS4wMDI5NkwxMC43Nzk5IDYuMjgzNDVDMTAuODEwNSA2LjUxNDE2IDEwLjk1MzEgNi42OTgwMiAxMS4xNTQ3IDYuODE0MzlMMTIuMTU0MSA3LjQwNzc5QzEyLjM1NTYgNy41MjQxNiAxMi42MjYxIDcuNTU5MDcgMTIuODQxMiA3LjQ3MDI1TDE0LjAyOCA2Ljk3MDU1TDE0LjYyMTQgOC4wMzI0MkwxMy42MjIgOC44MTMyQzEzLjQzNzUgOC45NTUwOSAxMy4zMDk2IDkuMTczODYgMTMuMzA5NiA5LjQwNjZMMTMuMzA5NiAxMC41OTM0QzEzLjMwOTYgMTAuODI2NCAxMy40MzcxIDExLjA0NDkgMTMuNjIyIDExLjE4NjhMMTQuNjIxNCAxMS45Njc2TDE0LjAyOCAxMy4wMjk1TDEyLjg0MTIgMTIuNTI5N0MxMi42MjYxIDEyLjQ0MDkgMTIuMzU1NiAxMi40NzU4IDEyLjE1NDEgMTIuNTkyMkwxMS4xNTQ3IDEzLjE4NTZDMTAuOTUzMSAxMy4zMDIgMTAuODEwNSAxMy40ODU4IDEwLjc3OTkgMTMuNzE2NUwxMC42MjM3IDE0Ljk5N0w5LjM3NDQ4IDE0Ljk5N0w5LjIxODMyIDEzLjcxNjVDOS4xODc2OSAxMy40ODU4IDkuMDQ1MDkgMTMuMzAyIDguODQzNTQgMTMuMTg1Nkw3Ljg0NDE0IDEyLjU5MjJDNy42NDI1OSAxMi40NzU4IDcuMzcyMTYgMTIuNDQwOSA3LjE1NzA0IDEyLjUyOTdMNS45NzAyNSAxMy4wMjk1TDUuMzc2ODUgMTEuOTY3Nkw2LjM3NjI2IDExLjE4NjhDNi41NjA3NSAxMS4wNDQ5IDYuNjg4NTcgMTAuODI2MSA2LjY4ODU3IDEwLjU5MzRMNi42ODg1NyA5LjQwNjZDNi42ODg1NyA5LjE3MzkzIDYuNTYwNjYgOC45NTUwOSA2LjM3NjI2IDguODEzMkw1LjM3Njg1IDguMDMyNDJMNS45NzAyNSA2Ljk3MDU1TDcuMTU3MDQgNy40NzAyNUM3LjM3MjMgNy41NTk0IDcuNjExMTIgNy41MjQyNiA3LjgxMjkgNy40MDc3OUw4Ljg0MzU0IDYuODE0MzlDOS4wNDUxMyA2LjY5ODAzIDkuMTg3NjggNi41MTQxOSA5LjIxODMyIDYuMjgzNDVMOS4zNzQ0OCA1LjAwMjk2Wk05Ljk5OTExIDguMjUxMDRDOS4wMzMxOCA4LjI1MTA0IDguMjUwMTQgOS4wMzQwNyA4LjI1MDE0IDEwQzguMjUwMTQgMTAuOTY1OSA5LjAzMzE4IDExLjc0OSA5Ljk5OTExIDExLjc0OUMxMC45NjUgMTEuNzQ5IDExLjc0ODEgMTAuOTY1OSAxMS43NDgxIDEwQzExLjc0ODEgOS4wMzQwNyAxMC45NjUgOC4yNTEwNCA5Ljk5OTExIDguMjUxMDRaIi8+Cjwvc3ZnPg==",
  );
  hubButton.textContent = "Floorp Hub";

  // Override activate() so clicking opens Floorp Hub instead of
  // dispatching a change-view event that would route to a non-existent pane.
  hubButton.activate = () => {
    openFloorpHub();
  };

  nav_root.insertBefore(hubButton, before_fragment);
}

function ensureFloorpHubWarning(): void {
  const initializeWarning = () => {
    logFloorpHub("Executing warning creation.");
    createFloorpHubWarning();
  };

  logFloorpHub("Scheduling warning creation.");

  if (doc.readyState === "loading") {
    logFloorpHub("Document loading; deferring until DOMContentLoaded.");
    doc.addEventListener("DOMContentLoaded", initializeWarning, {
      once: true,
    });
    return;
  }

  initializeWarning();
}

function createFloorpHubWarning(): void {
  logFloorpHub("Creating warning banner if necessary.");

  const paneContainer = doc.querySelector(
    ".pane-container",
  ) as XULElement | null;
  const mainPrefPane = doc.querySelector(
    "#mainPrefPane",
  ) as XULElement | null;
  if (!paneContainer || !mainPrefPane) {
    console.warn(
      "Floorp Hub:",
      "Pane container or mainPrefPane not found; cannot insert warning.",
    );
    return;
  }

  const existingContainer = doc.getElementById(
    FLOORP_HUB_WARNING_IDS.container,
  ) as XULElement | null;
  if (existingContainer) {
    logFloorpHub("Existing banner located; updating instead of recreating.");
    const messageElement = existingContainer.querySelector(
      `#${FLOORP_HUB_WARNING_IDS.message}`,
    ) as XULElement | null;
    const buttonElement = existingContainer.querySelector(
      `#${FLOORP_HUB_WARNING_IDS.button}`,
    ) as XULElement | null;

    if (messageElement && buttonElement) {
      floorpHubWarningElements = {
        container: existingContainer,
        message: messageElement,
        button: buttonElement,
      };
      bindLocaleObserverForWarning();
      applyFloorpHubWarningTexts();
      return;
    }

    logFloorpHub("Existing banner incomplete; removing for clean rebuild.");
    existingContainer.remove();
  }

  injectFloorpHubWarningStyles();

  const container = doc.createXULElement("hbox") as XULElement;
  container.id = FLOORP_HUB_WARNING_IDS.container;
  container.setAttribute("align", "center");
  container.setAttribute("pack", "start");
  container.setAttribute("role", "alert");
  container.setAttribute("aria-live", "polite");
  container.classList.add("floorp-hub-warning");

  const message = doc.createXULElement("description") as XULElement;
  message.id = FLOORP_HUB_WARNING_IDS.message;
  message.classList.add("floorp-hub-warning__message");
  message.setAttribute("flex", "1");
  message.textContent = defaultWarningTexts.message;

  const button = doc.createXULElement("button") as XULElement;
  button.id = FLOORP_HUB_WARNING_IDS.button;
  button.classList.add("floorp-hub-warning__button");
  button.setAttribute("label", defaultWarningTexts.buttonLabel);
  button.addEventListener("click", openFloorpHub);

  container.appendChild(message);
  container.appendChild(button);

  paneContainer.insertBefore(container, mainPrefPane);
  logFloorpHub(
    "Inserted warning banner before #mainPrefPane in .pane-container.",
  );

  floorpHubWarningElements = {
    container,
    message,
    button,
  };

  // Bind handlers and only apply i18n texts immediately if i18next is initialized.
  bindI18nHandlersForWarning();
  bindLocaleObserverForWarning();
  const instance = getI18nextInstance();
  if (instance && instance.isInitialized) {
    applyFloorpHubWarningTexts();
  } else {
    logFloorpHub("i18next not yet initialized; will apply texts when ready.");
  }
}

function logI18nextDiagnostics(instance: I18NextLike | null): void {
  if (!instance) {
    logFloorpHub("i18next diagnostics: instance is null");
    return;
  }

  try {
    const instObj = instance as unknown as Record<string, unknown>;
    const langs = (instObj["languages"] as string[] | undefined) ?? null;
    const opts =
      (instObj["options"] as Record<string, unknown> | undefined) ?? null;
    const existsFn = instObj["exists"] as ((k: string) => boolean) | undefined;
    const hasMessage =
      typeof existsFn === "function"
        ? existsFn("floorpHubWarning.message")
        : null;

    logFloorpHub("i18next diagnostics:", {
      isInitialized: instance.isInitialized,
      languages: langs,
      options: opts,
      hasMessageKey: hasMessage,
    });
  } catch (error) {
    console.error("Floorp Hub:", "Failed to run i18next diagnostics", error);
  }
}

function injectFloorpHubWarningStyles(): void {
  if (doc.getElementById(FLOORP_HUB_WARNING_IDS.style)) {
    logFloorpHub("Warning styles already present; skipping injection.");
    return;
  }

  logFloorpHub("Injecting warning styles.");
  const style = doc.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "style",
  );
  style.id = FLOORP_HUB_WARNING_IDS.style;
  style.textContent = `
    #${FLOORP_HUB_WARNING_IDS.container} {
      background-color: #fff4ce;
      border: 1px solid #f5c518;
      border-radius: 6px;
      padding: 12px 16px;
      margin-block: 12px;
      gap: 12px;
      color: #1f1f1f;
    }

    #${FLOORP_HUB_WARNING_IDS.container} .floorp-hub-warning__message {
      font-size: 1em;
      line-height: 1.4;
    }

    #${FLOORP_HUB_WARNING_IDS.container} .floorp-hub-warning__button {
      background-color: #ffda55;
      border: 1px solid #e0a800;
      border-radius: 4px;
      padding: 4px 12px;
      font-weight: 600;
      color: #1f1f1f;
    }

    #${FLOORP_HUB_WARNING_IDS.container} .floorp-hub-warning__button:hover {
      background-color: #ffd13b;
    }
    
    /* Blue card for Floorp Start (Home) warning - layout matches the yellow card */
    #${FLOORP_START_WARNING_IDS.container} {
      background-color: #e8f4ff; /* light blue */
      border: 1px solid #7db2ff;
      border-radius: 6px;
      padding: 12px 16px;
      margin-block: 12px;
      gap: 12px;
      color: #0b2540;
    }

    #${FLOORP_START_WARNING_IDS.container} .floorp-hub-warning__message {
      font-size: 1em;
      line-height: 1.4;
    }

    #${FLOORP_START_WARNING_IDS.container} .floorp-hub-warning__button {
      background-color: #4da3ff;
      border: 1px solid #2b8cff;
      border-radius: 4px;
      padding: 4px 12px;
      font-weight: 600;
      color: #ffffff;
    }

    #${FLOORP_START_WARNING_IDS.container} .floorp-hub-warning__button:hover {
      background-color: #3390ff;
    }
  `;

  const parent =
    doc.head ??
    (doc.querySelector("head") as HTMLElement | null) ??
    doc.documentElement;
  parent?.appendChild(style);
}
function bindI18nHandlersForWarning(): void {
  if (warningI18nHandlersBound) {
    logFloorpHub("i18n handlers already bound for warning banner.");
    return;
  }

  logFloorpHub("Binding i18n handlers for warning banner.");
  const instance = getI18nextInstance();
  if (!instance) {
    logFloorpHub("i18next instance not available; deferring i18n binding.");
    return;
  }

  try {
    // Prefer event binding if available
    if (typeof instance.on === "function") {
      instance.on("initialized", () => {
        applyFloorpHubWarningTexts();
        applyFloorpStartWarningTexts();
      });
      instance.on("languageChanged", () => {
        applyFloorpHubWarningTexts();
        applyFloorpStartWarningTexts();
      });
      warningI18nHandlersBound = true;
      logFloorpHub("Bound i18next event handlers for warning banner.");
      // If already initialized, apply immediately
      if ((instance as I18NextLike).isInitialized) {
        logFloorpHub("i18next is already initialized; applying texts now.");
        applyFloorpHubWarningTexts();
      }
    } else {
      // Fallback: poll for initialization for a short time
      logFloorpHub(
        "i18next instance has no .on; polling isInitialized for up to 5s.",
      );
      let checks = 0;
      const interval = setInterval(() => {
        checks++;
        try {
          if ((instance as I18NextLike).isInitialized) {
            clearInterval(interval);
            logFloorpHub(
              "i18next became initialized via poll; applying texts.",
            );
            applyFloorpHubWarningTexts();
            applyFloorpStartWarningTexts();
            warningI18nHandlersBound = true;
            return;
          }
        } catch {
          // ignore and continue polling
        }
        if (checks >= 25) {
          clearInterval(interval);
          logFloorpHub("Stopped polling for i18next initialization (timeout).");
        }
      }, 200);
    }
  } catch (error) {
    console.error("Floorp Hub:", "Failed to bind i18n handlers", error);
  }
}

function bindLocaleObserverForWarning(): void {
  if (localeObserverBound) {
    logFloorpHub("Locale observer already bound for warning banner.");
    return;
  }

  const utils = getI18nUtils();
  if (!utils) {
    logFloorpHub("I18nUtils unavailable; cannot bind locale observer.");
    return;
  }

  try {
    utils.addLocaleChangeListener(() => {
      logFloorpHub(
        "Locale change detected via I18nUtils; updating warning texts.",
      );
      applyFloorpHubWarningTexts();
    });
    localeObserverBound = true;
    logFloorpHub("Bound locale change observer for warning banner.");
  } catch (error) {
    console.error("Floorp Hub:", "Failed to bind locale observer", error);
  }
}

function applyFloorpHubWarningTexts(): void {
  if (!floorpHubWarningElements) {
    logFloorpHub("No warning elements to update.");
    return;
  }

  const texts = getFloorpHubWarningTexts();
  floorpHubWarningElements.message.textContent = texts.message;
  floorpHubWarningElements.button.setAttribute("label", texts.buttonLabel);
  logFloorpHub("Applied warning texts:", texts);
}

function getFloorpHubWarningTexts(): FloorpHubWarningTexts {
  const instance = getI18nextInstance();
  if (instance && instance.isInitialized) {
    return {
      message: translateWithFallback(
        instance,
        "floorpHubWarning.message",
        defaultWarningTexts.message,
      ),
      buttonLabel: translateWithFallback(
        instance,
        "floorpHubWarning.openButton",
        defaultWarningTexts.buttonLabel,
      ),
    };
  }

  // Attempt to use a registered translation provider via I18nUtils. This
  // avoids races: the UI bundle can register i18next.t on I18nUtils and
  // chrome scripts can call it synchronously when available.
  const utils = getI18nUtils();
  try {
    const provider = utils?.getTranslationProvider?.();
    if (provider && typeof provider.t === "function") {
      try {
        const msg = provider.t("floorpHubWarning.message", {
          defaultValue: defaultWarningTexts.message,
        });
        const btn = provider.t("floorpHubWarning.openButton", {
          defaultValue: defaultWarningTexts.buttonLabel,
        });
        return {
          message: Array.isArray(msg)
            ? msg.join(" ")
            : String(msg) || defaultWarningTexts.message,
          buttonLabel: Array.isArray(btn)
            ? btn.join(" ")
            : String(btn) || defaultWarningTexts.buttonLabel,
        };
      } catch {
        // fall through to locale fallback
      }
    }
  } catch {
    // ignore provider errors and fall back to localized map
  }

  logFloorpHub("No localized fallback in preferences; using default texts.");
  return defaultWarningTexts;
}

function translateWithFallback(
  instance: I18NextLike,
  key: string,
  fallback: string,
): string {
  const translated = instance.t(key, { defaultValue: fallback });
  if (Array.isArray(translated)) {
    logFloorpHub("Translation result array for key:", key);
    return translated.join(" ");
  }

  if (typeof translated === "string" && translated.trim().length > 0) {
    logFloorpHub("Translation resolved for key:", key, "->", translated);
    return translated;
  }

  logFloorpHub("Translation empty; returning fallback for key:", key);
  return fallback;
}

// Localized fallback helper removed — preferences intentionally do not use
// localized fallback maps and will display default texts when translations
// are unavailable.

/* ===== Floorp Start (Home) warning ===== */
const FLOORP_START_WARNING_IDS = {
  container: "floorp-start-warning",
  message: "floorp-start-warning-message",
  button: "floorp-start-warning-button",
} as const;

const defaultStartWarningTexts: FloorpHubWarningTexts = {
  message:
    'To change the "Home" setting, disable Floorp Start from Floorp Hub (about:hub#/features/design).',
  buttonLabel: "Open Floorp Hub — Design",
};

// Localized start warning map removed for preferences — use default texts.

let floorpStartWarningElements: {
  container: XULElement;
  message: XULElement;
  button: XULElement;
} | null = null;

function getFloorpStartWarningTexts(): FloorpHubWarningTexts {
  const instance = getI18nextInstance();
  if (instance && instance.isInitialized) {
    return {
      message: translateWithFallback(
        instance,
        "floorpStartWarning.message",
        defaultStartWarningTexts.message,
      ),
      buttonLabel: translateWithFallback(
        instance,
        "floorpStartWarning.openButton",
        defaultStartWarningTexts.buttonLabel,
      ),
    };
  }

  // Try translation provider via I18nUtils first (synchronous short-path)
  const utils = getI18nUtils();
  try {
    const provider = utils?.getTranslationProvider?.();
    if (provider && typeof provider.t === "function") {
      try {
        const msg = provider.t("floorpStartWarning.message", {
          defaultValue: defaultStartWarningTexts.message,
        });
        const btn = provider.t("floorpStartWarning.openButton", {
          defaultValue: defaultStartWarningTexts.buttonLabel,
        });
        return {
          message: Array.isArray(msg)
            ? msg.join(" ")
            : String(msg) || defaultStartWarningTexts.message,
          buttonLabel: Array.isArray(btn)
            ? btn.join(" ")
            : String(btn) || defaultStartWarningTexts.buttonLabel,
        };
      } catch {
        // fall through to localized fallback below
      }
    }
  } catch {
    // ignore provider errors and fall back to localized map
  }

  logFloorpHub(
    "No localized fallback for Floorp Start in preferences; using default texts.",
  );
  return defaultStartWarningTexts;
}

function ensureFloorpStartWarning(): void {
  const initialize = () => createFloorpStartWarning();

  if (doc.readyState === "loading") {
    doc.addEventListener("DOMContentLoaded", initialize, { once: true });
    return;
  }

  initialize();
}

function createFloorpStartWarning(): void {
  const existing = doc.getElementById(
    FLOORP_START_WARNING_IDS.container,
  ) as XULElement | null;
  if (existing) return;

  const container = doc.createXULElement("hbox") as XULElement;
  container.id = FLOORP_START_WARNING_IDS.container;
  container.setAttribute("align", "center");
  container.setAttribute("pack", "start");
  container.setAttribute("role", "alert");
  container.setAttribute("aria-live", "polite");
  container.classList.add("floorp-hub-warning");

  const message = doc.createXULElement("description") as XULElement;
  message.id = FLOORP_START_WARNING_IDS.message;
  message.classList.add("floorp-hub-warning__message");
  message.setAttribute("flex", "1");
  message.textContent = defaultStartWarningTexts.message;

  const button = doc.createXULElement("button") as XULElement;
  button.id = FLOORP_START_WARNING_IDS.button;
  button.classList.add("floorp-hub-warning__button");
  button.setAttribute("label", defaultStartWarningTexts.buttonLabel);
  button.addEventListener("click", () => {
    if (import.meta.env.MODE === "dev") {
      (globalThis as unknown as Window).location.href =
        "http://localhost:5183/#/features/design";
      return;
    }
    const win = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as FirefoxWindow | null;
    if (!win) {
      console.error(
        "Floorp Hub: No browser window available to open hub tab.",
      );
      return;
    }
    win.gBrowser.selectedTab = win.gBrowser.addTab(
      "about:hub#/features/design",
      {
        relatedToCurrent: true,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      },
    );
  });

  container.appendChild(message);
  container.appendChild(button);

  const paneContainer = doc.querySelector(
    ".pane-container",
  ) as XULElement | null;
  const mainPrefPane = doc.querySelector(
    "#mainPrefPane",
  ) as XULElement | null;
  if (paneContainer && mainPrefPane) {
    // Insert the start warning after the hub warning (if present), or
    // right before #mainPrefPane.
    const hubWarning = doc.getElementById(FLOORP_HUB_WARNING_IDS.container);
    if (hubWarning && hubWarning.parentElement === paneContainer) {
      paneContainer.insertBefore(container, hubWarning.nextSibling);
    } else {
      paneContainer.insertBefore(container, mainPrefPane);
    }
  } else {
    console.warn(
      "Floorp Hub:",
      ".pane-container or #mainPrefPane not found; inserting warning at document top.",
    );
    const parent = doc.body ?? doc.documentElement;
    parent!.insertBefore(container, parent!.firstChild);
  }

  floorpStartWarningElements = { container, message, button };
  bindI18nHandlersForWarning();
  bindLocaleObserverForWarning();
  applyFloorpStartWarningTexts();
}

function applyFloorpStartWarningTexts(): void {
  if (!floorpStartWarningElements) return;
  const texts = getFloorpStartWarningTexts();
  floorpStartWarningElements.message.textContent = texts.message;
  floorpStartWarningElements.button.setAttribute("label", texts.buttonLabel);
}

// Primary-locale helper removed as localized fallback behavior was removed
// from preferences.

function getI18nUtils(): {
  getPrimaryBrowserLocaleMapped(): string;
  addLocaleChangeListener(callback: (locale: string) => void): void;
  // Optional translation provider APIs added to I18nUtils
  getTranslationProvider?: () => {
    t?: (key: string, opts?: Record<string, unknown>) => string | string[];
    isInitializedPromise?: Promise<unknown>;
  } | null;
  registerTranslationProvider?: (provider: {
    t?: (key: string, opts?: Record<string, unknown>) => string | string[];
    isInitializedPromise?: Promise<unknown>;
    changeLanguage?: (lng: string) => Promise<unknown> | void;
    getI18next?: () => unknown;
  }) => void;
} | null {
  if (i18nUtilsInstance !== undefined) {
    return i18nUtilsInstance;
  }

  try {
    const module = ChromeUtils.importESModule(
      "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
    ) as {
      I18nUtils: {
        getPrimaryBrowserLocaleMapped(): string;
        addLocaleChangeListener(callback: (locale: string) => void): void;
      };
    };
    i18nUtilsInstance = module.I18nUtils;
    logFloorpHub("Retrieved I18nUtils instance.");
  } catch (error) {
    console.error("Floorp Hub:", "Failed to import I18n-Utils.sys.mjs", error);
    i18nUtilsInstance = null;
  }

  return i18nUtilsInstance ?? null;
}

function hideNewTabPage(): void {
  let config = false;
  try {
    const pref = Services.prefs.getStringPref(
      "floorp.design.configs",
      "{}",
    );
    config =
      (JSON.parse(pref) as {
        uiCustomization?: { disableFloorpStart?: boolean };
      })?.uiCustomization?.disableFloorpStart ?? false;
  } catch (e) {
    console.error(
      "Floorp Hub: Failed to read floorp.design.configs",
      e,
    );
  }

  if (!config) {
    logFloorpHub("Disabling Floorp start categories as per configuration.");
    doc
      .querySelectorAll('#category-home, [data-category="paneHome"]')
      .forEach((el: Element) => {
        el.remove();
      });
  }

  ensureFloorpHubWarning();
}

ensureI18NInitialized();
addFloorpHubCategory();
ensureFloorpHubWarning();
ensureFloorpStartWarning();
Services.obs.addObserver(hideNewTabPage, "home-pane-loaded");
