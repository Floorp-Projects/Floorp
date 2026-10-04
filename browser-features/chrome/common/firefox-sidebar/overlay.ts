// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import { createSignal } from "solid-js";
import type {
  NativeSidebarController,
  SidebarOverlaySettings,
} from "./types.ts";

const OPEN_DELAY = 180;
const CLOSE_DELAY = 350;
const XHTML = "http://www.w3.org/1999/xhtml";
const ROOT_ATTRIBUTES = [
  "floorp-firefox-sidebar-overlay",
  "floorp-firefox-sidebar-hover",
] as const;
const VARIABLES = [
  "--floorp-firefox-sidebar-left",
  "--floorp-firefox-sidebar-top",
  "--floorp-firefox-sidebar-height",
  "--floorp-firefox-sidebar-available-width",
  "--floorp-firefox-sidebar-trigger-left",
  "--floorp-firefox-launcher-left",
  "--floorp-firefox-launcher-reserve-start",
  "--floorp-firefox-launcher-reserve-end",
] as const;

/** Native SidebarController remains responsible for the selected panel and its
 * document. Hover collapse only hides that document, so it cannot discard forms,
 * unload an extension, steal focus, or change the site's viewport. */
export class FirefoxSidebarOverlayController {
  private readonly state = createSignal({ expanded: false, pinned: false });
  private settings: SidebarOverlaySettings = { overlay: false, hover: false };
  private browser: HTMLElement | null = null;
  private box: HTMLElement | null = null;
  private launcher: HTMLElement | null = null;
  private content: HTMLElement | null = null;
  private trigger: HTMLButtonElement | null = null;
  private resizer: HTMLDivElement | null = null;
  private disposed = false;
  private ready = false;
  private hovered = false;
  private escaped = false;
  private opening = false;
  private hoverOpeningCommand: string | undefined;
  private openingLauncherWasHidden = false;
  private ignoreNextNativeClose = false;
  private requestGeneration = 0;
  private resizing = false;
  private resizeCleanup: (() => void) | undefined;
  private railSpace = 0;
  private openTimer: number | undefined;
  private closeTimer: number | undefined;
  private frame: number | undefined;
  private readonly popups = new Set<Element>();
  private resizeObserver: ResizeObserver | undefined;
  private mutationObserver: MutationObserver | undefined;
  private readonly listeners: Array<() => void> = [];

  constructor(private readonly native: NativeSidebarController) {
    native.promiseInitialized.then(() => {
      if (this.disposed || native.inSingleTabWindow) return;
      this.browser = document.getElementById("browser");
      this.box = document.getElementById("sidebar-box");
      this.launcher = document.getElementById("sidebar-container");
      this.content = document.getElementById("tabbrowser-tabbox");
      if (!this.browser || !this.box || !this.content) return;
      this.attach();
      this.ready = true;
      this.applySettings();
    }).catch((error: unknown) => console.error("[FirefoxSidebar]", error));
  }

  configure(settings: SidebarOverlaySettings): void {
    if (
      settings.overlay !== this.settings.overlay ||
      settings.hover !== this.settings.hover
    ) this.invalidateOpening();
    this.settings = settings;
    this.applySettings();
  }

  get expanded(): boolean {
    return this.state[0]().expanded;
  }

  get pinned(): boolean {
    return this.state[0]().pinned;
  }

  private listen(
    target: EventTarget,
    type: string,
    listener: EventListener,
    capture = false,
  ): void {
    target.addEventListener(type, listener, capture);
    this.listeners.push(() =>
      target.removeEventListener(type, listener, capture)
    );
  }

  private attach(): void {
    const { browser, box } = this;
    if (!browser || !box) return;
    const trigger = document.createElementNS(
      XHTML,
      "button",
    ) as HTMLButtonElement;
    trigger.id = "floorp-firefox-sidebar-edge-toggle";
    trigger.type = "button";
    trigger.setAttribute("aria-controls", "sidebar-box");
    trigger.setAttribute("aria-pressed", "false");
    browser.appendChild(trigger);
    this.trigger = trigger;

    const resizer = document.createElementNS(XHTML, "div") as HTMLDivElement;
    resizer.id = "floorp-firefox-sidebar-overlay-resizer";
    resizer.tabIndex = 0;
    resizer.setAttribute("role", "separator");
    resizer.setAttribute("aria-orientation", "vertical");
    box.appendChild(resizer);
    this.resizer = resizer;
    const translate = () => {
      trigger.setAttribute(
        "aria-label",
        i18next.t("firefoxSidebar.edgeToggle", {
          ns: "browser-chrome",
        }),
      );
      trigger.title = trigger.getAttribute("aria-label") ?? "";
      resizer.setAttribute(
        "aria-label",
        i18next.t("firefoxSidebar.resize", {
          ns: "browser-chrome",
        }),
      );
    };
    translate();
    i18next.on("languageChanged", translate);
    this.listeners.push(() => i18next.off("languageChanged", translate));

    for (const element of [trigger, box, this.launcher]) {
      if (!element) continue;
      this.listen(element, "mouseenter", () => this.enter());
      this.listen(element, "mouseleave", (event) => {
        if (!this.contains((event as MouseEvent).relatedTarget)) this.leave();
      });
    }
    this.listen(trigger, "click", (event) => {
      if (
        !this.settings.hover ||
        document.documentElement.hasAttribute("inDOMFullscreen")
      ) return;
      this.escaped = false;
      const pinned = !this.pinned;
      if (pinned) this.hoverOpeningCommand = undefined;
      this.setState(pinned || this.hovered, pinned);
      if (pinned) void this.showPanel();
      else {
        if (
          (event as MouseEvent).detail > 0 && document.activeElement === trigger
        ) {
          (globalThis as unknown as {
            gBrowser?: { selectedBrowser: { focus(): void } };
          })
            .gBrowser?.selectedBrowser.focus();
        }
        this.scheduleClose();
      }
    });
    this.listen(document, "focusin", (event) => {
      if (
        !this.settings.hover ||
        document.documentElement.hasAttribute("inDOMFullscreen")
      ) return;
      if (this.contains(event.target)) {
        if (event.target === trigger) this.escaped = false;
        this.clearCloseTimer();
        if (!this.escaped) {
          this.setState(true, this.pinned);
          void this.showPanel();
        }
      } else this.scheduleClose();
    }, true);
    this.listen(document, "focusout", () => this.scheduleClose(), true);
    this.listen(box, "SidebarShown", () => {
      if (this.settings.hover && !this.opening) {
        this.escaped = false;
        this.clearTimers();
        this.setState(true, true);
      }
    });
    this.listen(document, "keydown", (event) => {
      const key = event as KeyboardEvent;
      if (
        key.defaultPrevented ||
        !this.settings.hover || key.key !== "Escape" || !this.expanded ||
        this.popups.size
      ) return;
      this.escaped = true;
      this.clearTimers();
      this.setState(false, false);
      // A collapsed panel must never retain keyboard focus in invisible content.
      if (this.box?.contains(document.activeElement)) {
        (document.getElementById("tabbrowser-tabpanels") as HTMLElement | null)
          ?.focus();
        (globalThis as unknown as {
          gBrowser?: { selectedBrowser: { focus(): void } };
        })
          .gBrowser?.selectedBrowser.focus();
      }
    });
    this.listen(document, "popupshown", (event) => {
      const target = event.target;
      if (
        this.expanded && target instanceof Element &&
        target.localName !== "tooltip"
      ) {
        this.popups.add(target);
        this.clearCloseTimer();
      }
    }, true);
    this.listen(document, "popuphidden", (event) => {
      if (event.target instanceof Element) this.popups.delete(event.target);
      this.scheduleClose();
    }, true);
    this.listen(
      resizer,
      "mousedown",
      (event) => this.startResize(event as MouseEvent),
    );
    this.listen(resizer, "keydown", (event) => {
      const key = event as KeyboardEvent;
      if (key.key !== "ArrowLeft" && key.key !== "ArrowRight") return;
      key.preventDefault();
      const step = key.shiftKey ? 50 : 10;
      const delta = key.key === "ArrowRight" ? step : -step;
      this.resizeTo(
        (box.getBoundingClientRect().width) + (this.onRight() ? -delta : delta),
      );
    });
    this.listen(window, "resize", () => this.scheduleGeometry());
    this.listen(window, "blur", () => {
      this.hovered = false;
      if (this.openTimer !== undefined) clearTimeout(this.openTimer);
      this.openTimer = undefined;
      this.scheduleClose();
    });
    this.resizeObserver = new ResizeObserver(() => this.scheduleGeometry());
    for (const element of [browser, box, this.launcher, this.content]) {
      if (element) this.resizeObserver.observe(element);
    }
    this.mutationObserver = new MutationObserver((records) => {
      if (
        records.some((record) =>
          record.target === box && record.attributeName === "hidden"
        )
      ) {
        if (!this.native.isOpen) {
          if (this.ignoreNextNativeClose) {
            this.ignoreNextNativeClose = false;
            this.scheduleGeometry();
            return;
          }
          // Native close buttons remain authoritative. Require a new hover gesture
          // before reopening, including when the pointer is still over the rail.
          this.escaped = true;
          this.requestGeneration++;
          this.setState(false, false);
          this.clearTimers();
        } else if (this.native.isOpen && !this.opening && this.settings.hover) {
          this.escaped = false;
          this.setState(true, true);
        }
      }
      if (document.documentElement.hasAttribute("inDOMFullscreen")) {
        this.invalidateOpening();
        this.clearTimers();
        this.setState(false, this.pinned);
      } else if (
        records.some((record) =>
          record.target === document.documentElement &&
          record.attributeName === "inDOMFullscreen"
        ) &&
        this.settings.hover && this.pinned && this.native.isOpen
      ) {
        this.setState(true, true);
      }
      this.scheduleGeometry();
    });
    this.mutationObserver.observe(document.documentElement, {
      attributes: true,
      attributeFilter: [
        "inDOMFullscreen",
        "inFullscreen",
        "dir",
        "fullscreenNavToolboxHidden",
      ],
    });
    this.mutationObserver.observe(box, {
      attributes: true,
      attributeFilter: [
        "hidden",
        "sidebar-positionend",
        "positionend",
        "style",
      ],
    });
    if (this.launcher) {
      this.mutationObserver.observe(this.launcher, {
        attributes: true,
        attributeFilter: [
          "hidden",
          "sidebar-launcher-expanded",
          "sidebar-ongoing-animations",
        ],
      });
    }
  }

  private captureRailSpace(): void {
    if (!this.launcher || !this.content || this.launcher.hidden) {
      this.railSpace = 0;
      return;
    }
    const rail = this.launcher.getBoundingClientRect();
    const content = this.content.getBoundingClientRect();
    this.railSpace = Math.max(
      0,
      this.onRight() ? rail.right - content.right : content.left - rail.left,
    );
    // If the panel was still in flow when this setting changed, it is removed
    // separately by overlay mode; reserve only the launcher's real outer width.
    if (
      this.box && getComputedStyle(this.box)?.position !== "absolute" &&
      this.native.isOpen
    ) {
      this.railSpace = Math.max(
        0,
        this.railSpace - this.box.getBoundingClientRect().width -
          (document.getElementById("sidebar-splitter")?.getBoundingClientRect()
            .width ?? 0),
      );
    }
  }

  private applySettings(): void {
    if (!this.ready || !this.browser || !this.box) return;
    const root = document.documentElement;
    const wasHover = root.hasAttribute(ROOT_ATTRIBUTES[1]);
    if (this.settings.overlay && !root.hasAttribute(ROOT_ATTRIBUTES[0])) {
      this.captureRailSpace();
    }
    if (this.settings.hover && !wasHover) {
      this.escaped = false;
      this.setState(false, false);
    }
    root.toggleAttribute(ROOT_ATTRIBUTES[0], this.settings.overlay);
    root.toggleAttribute(ROOT_ATTRIBUTES[1], this.settings.hover);
    if (!this.settings.hover) {
      this.clearTimers();
      this.escaped = false;
      this.setState(true, false);
      this.box.removeAttribute("data-floorp-sidebar-expanded");
    }
    this.trigger?.toggleAttribute("hidden", !this.settings.hover);
    this.resizer?.toggleAttribute("hidden", !this.settings.overlay);
    this.updateGeometry();
    this.scheduleGeometry();
  }

  private onRight(): boolean {
    const atEnd = this.box?.hasAttribute("sidebar-positionend") ||
      this.box?.hasAttribute("positionend");
    return Boolean(atEnd) !==
      (getComputedStyle(document.documentElement)?.direction === "rtl");
  }

  private contains(target: EventTarget | null): boolean {
    if (!(target instanceof Node)) return false;
    return Boolean(
      this.box?.contains(target) || this.trigger?.contains(target) ||
        this.launcher?.contains(target),
    );
  }

  private focused(): boolean {
    return document.hasFocus() && this.contains(document.activeElement);
  }

  private setState(expanded: boolean, pinned: boolean): void {
    this.state[1]({ expanded, pinned });
    this.box?.toggleAttribute("data-floorp-sidebar-expanded", expanded);
    this.trigger?.setAttribute(
      "aria-expanded",
      String(expanded && this.native.isOpen),
    );
    this.trigger?.setAttribute("aria-pressed", String(pinned));
    this.scheduleGeometry();
  }

  private enter(): void {
    if (
      !this.settings.hover ||
      document.documentElement.hasAttribute("inDOMFullscreen")
    ) return;
    this.hovered = true;
    this.clearCloseTimer();
    if (this.escaped || this.openTimer !== undefined) return;
    this.openTimer = globalThis.setTimeout(() => {
      this.openTimer = undefined;
      if (!this.hovered || this.disposed) return;
      this.setState(true, this.pinned);
      void this.showPanel();
    }, OPEN_DELAY);
  }

  private leave(): void {
    this.hovered = false;
    this.escaped = false;
    if (this.openTimer !== undefined) clearTimeout(this.openTimer);
    this.openTimer = undefined;
    this.scheduleClose();
  }

  private async showPanel(): Promise<void> {
    if (
      !this.settings.overlay || !this.settings.hover ||
      document.documentElement.hasAttribute("inDOMFullscreen") ||
      this.native.isOpen || this.opening || this.disposed
    ) return;
    const last = this.native.lastOpenedId;
    const commandID = last && this.native.sidebars.has(last)
      ? last
      : this.native.sidebars.has("viewBookmarksSidebar")
      ? "viewBookmarksSidebar"
      : [...this.native.sidebars].find(([, sidebar]) =>
        sidebar.visible !== false
      )?.[0];
    if (!commandID) return;
    const generation = this.requestGeneration;
    this.hoverOpeningCommand = this.pinned ? undefined : commandID;
    this.openingLauncherWasHidden = Boolean(this.launcher?.hidden);
    this.opening = true;
    try {
      await this.native.showInitially(commandID);
      if (this.disposed || generation !== this.requestGeneration) {
        this.closeOwnedOpening();
      } else {
        this.trigger?.setAttribute(
          "aria-expanded",
          String(this.expanded && this.native.isOpen),
        );
        this.scheduleClose();
        this.scheduleGeometry();
      }
    } catch (error) {
      console.error("[FirefoxSidebar]", error);
    } finally {
      this.opening = false;
      this.hoverOpeningCommand = undefined;
      if (
        generation !== this.requestGeneration &&
        !this.disposed && this.settings.overlay && this.settings.hover &&
        !this.escaped && !this.native.isOpen &&
        !document.documentElement.hasAttribute("inDOMFullscreen") &&
        (this.hovered || this.pinned || this.focused())
      ) {
        this.setState(true, this.pinned);
        void this.showPanel();
      }
    }
  }

  private invalidateOpening(): void {
    this.requestGeneration++;
    // _show() unhides the native box before its document finishes loading.
    // Close that transient opening before removing overlay positioning.
    this.closeOwnedOpening();
  }

  private closeOwnedOpening(): void {
    if (
      !this.opening || !this.hoverOpeningCommand || !this.native.isOpen ||
      this.native.currentID !== this.hoverOpeningCommand
    ) return;
    this.ignoreNextNativeClose = true;
    this.native.hide({ dismissPanel: false });
    if (this.native.isOpen) this.ignoreNextNativeClose = false;
    // Automatic native opening may have revealed a launcher the user had hidden.
    // Restore it through the native toolbar action while it is still overlaid.
    if (
      this.openingLauncherWasHidden && this.launcher && !this.launcher.hidden
    ) {
      void this.native.handleToolbarButtonClick?.().catch((error: unknown) =>
        console.error("[FirefoxSidebar]", error)
      );
    }
  }

  private scheduleClose(): void {
    if (!this.settings.hover || this.disposed) return;
    this.clearCloseTimer();
    this.closeTimer = globalThis.setTimeout(() => {
      this.closeTimer = undefined;
      if (
        this.hovered || this.focused() || this.pinned || this.popups.size ||
        this.resizing
      ) return;
      this.setState(false, false);
    }, CLOSE_DELAY);
  }

  private clearCloseTimer(): void {
    if (this.closeTimer !== undefined) clearTimeout(this.closeTimer);
    this.closeTimer = undefined;
  }

  private clearTimers(): void {
    if (this.openTimer !== undefined) clearTimeout(this.openTimer);
    this.openTimer = undefined;
    this.clearCloseTimer();
  }

  private scheduleGeometry(): void {
    if (!this.ready || this.disposed || this.frame !== undefined) return;
    this.frame = requestAnimationFrame(() => {
      this.frame = undefined;
      this.updateGeometry();
    });
  }

  private updateGeometry(): void {
    if (!this.browser || !this.content || !this.box) return;
    const right = this.onRight();
    const reserve = this.settings.overlay &&
        !document.documentElement.hasAttribute("inDOMFullscreen")
      ? this.railSpace
      : 0;
    this.browser.style.setProperty(
      "--floorp-firefox-launcher-reserve-start",
      `${!right ? reserve : 0}px`,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-launcher-reserve-end",
      `${right ? reserve : 0}px`,
    );
    const browser = this.browser.getBoundingClientRect();
    const content = this.content.getBoundingClientRect();
    const panelWidth = this.box.getBoundingClientRect().width;
    const launcherWidth = this.launcher?.getBoundingClientRect().width ?? 0;
    const overlayRailWidth = Math.max(0, launcherWidth - reserve);
    const left = Math.max(
      0,
      (right
        ? content.right - panelWidth - overlayRailWidth
        : content.left + overlayRailWidth) - browser.left,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-sidebar-left",
      `${left}px`,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-sidebar-top",
      `${content.top - browser.top}px`,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-sidebar-height",
      `${content.height}px`,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-sidebar-available-width",
      `${Math.max(0, content.width - overlayRailWidth)}px`,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-sidebar-trigger-left",
      `${
        Math.max(0, (right ? content.right - 8 : content.left) - browser.left)
      }px`,
    );
    this.browser.style.setProperty(
      "--floorp-firefox-launcher-left",
      `${
        (right
          ? content.right + reserve - launcherWidth
          : content.left - reserve) - browser.left
      }px`,
    );
    this.box.toggleAttribute("data-floorp-sidebar-right", right);
    this.resizer?.setAttribute("aria-valuenow", String(Math.round(panelWidth)));
    this.resizer?.setAttribute("aria-valuemin", "160");
    this.resizer?.setAttribute(
      "aria-valuemax",
      String(Math.round(content.width)),
    );
  }

  private resizeTo(width: number): void {
    if (!this.box || !this.content) return;
    const max = this.content.getBoundingClientRect().width;
    const min = Math.min(160, max);
    this.box.style.width = `${Math.max(min, Math.min(max, width))}px`;
    this.scheduleGeometry();
  }

  private startResize(event: MouseEvent): void {
    if (event.button !== 0 || !this.settings.overlay || !this.box) return;
    event.preventDefault();
    this.resizeCleanup?.();
    this.resizing = true;
    this.clearCloseTimer();
    const start = event.clientX;
    const width = this.box.getBoundingClientRect().width;
    const right = this.onRight();
    const move = (event: MouseEvent) =>
      this.resizeTo(width + (event.clientX - start) * (right ? -1 : 1));
    const end = () => {
      globalThis.removeEventListener("mousemove", move, true);
      globalThis.removeEventListener("mouseup", end, true);
      globalThis.removeEventListener("blur", end);
      this.resizing = false;
      this.resizeCleanup = undefined;
      this.scheduleClose();
    };
    globalThis.addEventListener("mousemove", move, true);
    globalThis.addEventListener("mouseup", end, true);
    globalThis.addEventListener("blur", end);
    this.resizeCleanup = end;
  }

  destroy(): void {
    this.invalidateOpening();
    this.disposed = true;
    this.resizeCleanup?.();
    this.clearTimers();
    if (this.frame !== undefined) cancelAnimationFrame(this.frame);
    this.frame = undefined;
    this.resizeObserver?.disconnect();
    this.mutationObserver?.disconnect();
    for (const remove of this.listeners) remove();
    this.listeners.length = 0;
    this.trigger?.remove();
    this.resizer?.remove();
    for (const attribute of ROOT_ATTRIBUTES) {
      document.documentElement.removeAttribute(attribute);
    }
    this.box?.removeAttribute("data-floorp-sidebar-expanded");
    this.box?.removeAttribute("data-floorp-sidebar-right");
    for (const variable of VARIABLES) {
      this.browser?.style.removeProperty(variable);
    }
  }
}
