// SPDX-License-Identifier: MPL-2.0
// Copied into the disposable test profile by --actor-module. The finite command
// set only navigates, inspects, or sends actual trusted input to native UI.
export class UserScriptsConsentUIChild extends JSWindowActorChild {
  /** @param {HTMLButtonElement|null|undefined} element @returns {import("./user-scripts-consent-test-types.ts").UserScriptsTrustedInput[]} */
  click(element) {
    const window = this.contentWindow, document = this.document;
    if (!window || !document || !element || element.disabled) {
      throw Error("Native consent control missing or disabled");
    }
    element.scrollIntoView({ block: "center", inline: "center" });
    const rect = element.getBoundingClientRect();
    if (!rect.width || !rect.height) {
      throw Error("Native consent control is not visible");
    }
    const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
    const hit = document.elementFromPoint(x, y), root = element.getRootNode();
    const host = "host" in root ? root.host : null;
    if (hit !== element && !element.contains(hit) && hit !== host) {
      throw Error("Native consent control is occluded");
    }
    /** @type {import("./user-scripts-consent-test-types.ts").UserScriptsTrustedInput[]} */
    const events = [];
    /** @param {Event} event */
    const observe = (event) =>
      events.push({ type: event.type, trusted: event.isTrusted });
    element.addEventListener("mousedown", observe);
    element.addEventListener("click", observe);
    try {
      window.synthesizeMouseEvent("mousedown", x, y, {
        button: 0,
        buttons: 1,
        clickCount: 1,
      });
      window.synthesizeMouseEvent("mouseup", x, y, {
        button: 0,
        buttons: 0,
        clickCount: 1,
      });
      return events;
    } finally {
      element.removeEventListener("mousedown", observe);
      element.removeEventListener("click", observe);
    }
  }

  /** @param {import("./user-scripts-consent-test-types.ts").UserScriptsConsentActorMessage} message */
  async receiveMessage(message) {
    const document = this.document, window = this.contentWindow;
    if (
      !document || !window || document.documentURI !== "about:addons" ||
      window.document !== document
    ) {
      throw Error(
        "The private consent actor requires the current Add-ons Manager document",
      );
    }
    const id = message.data?.id;
    if (typeof id !== "string" || !id.length || id.length > 256) {
      throw Error("Invalid private consent UI query");
    }
    const page =
      /** @type {import("./user-scripts-consent-test-types.ts").UserScriptsConsentNativePage} */ (window
        .wrappedJSObject);
    if (message.name === "Initialized") {
      return !!page.gViewController && !!page.promiseInitialized;
    }
    if (message.name === "Navigate") {
      if (!page.promiseInitialized || !page.gViewController) {
        throw Error("Native Add-ons Manager is not initialized");
      }
      await page.promiseInitialized;
      await page.gViewController.loadView("detail/" + id);
      return true;
    }
    const card = [...page.document.querySelectorAll("addon-card")].find(
      (card) => card.getAttribute("addon-id") === id,
    );
    if (message.name === "PermissionsVisible") {
      return !!card?.querySelector('button[name="permissions"]')
        ?.getBoundingClientRect().width;
    }
    if (message.name === "ClickPermissions") {
      return this.click(
        /** @type {HTMLButtonElement|null|undefined} */ (card?.querySelector(
          'button[name="permissions"]',
        )),
      );
    }
    if (message.name === "PanelVisible") {
      return !!card?.querySelector("addon-permissions-list")
        ?.getBoundingClientRect().height;
    }
    const component =
      /** @type {HTMLElement|null|undefined} */ (card?.querySelector(
        "floorp-chrome-user-scripts-toggle",
      ));
    const toggle =
      /** @type {import("./user-scripts-consent-test-types.ts").UserScriptsConsentNativeToggle|null|undefined} */ (component
        ?.querySelector("[data-chrome-user-scripts-allow]"));
    if (message.name === "ClickToggle") return this.click(toggle?.buttonEl);
    if (message.name !== "Snapshot") {
      throw Error("Unknown private consent UI command");
    }
    if (toggle?.updateComplete) await toggle.updateComplete;
    const rect = toggle?.buttonEl?.getBoundingClientRect();
    return {
      url: page.location.href,
      addonId: card?.getAttribute("addon-id") ?? null,
      componentPresent: !!component,
      componentHidden: !component || component.hidden,
      togglePresent: !!toggle,
      toggleVisible: !!(rect?.width && rect?.height && !component?.hidden),
      pressed: toggle ? !!toggle.pressed : null,
      disabled: toggle ? !!toggle.disabled : null,
      label: toggle
        ? (toggle.label || toggle.labelEl?.textContent || "").trim()
        : null,
    };
  }
}
