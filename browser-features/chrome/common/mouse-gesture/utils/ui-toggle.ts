// SPDX-License-Identifier: MPL-2.0
// Shared UI toggle utilities for keyboard shortcuts and mouse gestures.
// Extracted from legacy gFloorpDesign (Floorp-core/browser/base/content/browser-design.mjs).

/**
 * Toggle visibility of all `#navigator-toolbox` children.
 * When restoring, ensures the navigation bar remains visible if multiple
 * elements are shown.
 */
export function toggleUserInterface(doc: Document): void {
  try {
    const toolbox = doc.getElementById("navigator-toolbox");
    if (!toolbox) return;

    let shownElementAmount = 0;
    for (const child of toolbox.children) {
      const el = child as HTMLElement;
      el.style.display = el.style.display ? "" : "none";
      if (el.style.display === "") {
        shownElementAmount++;
      }
    }

    if (toolbox.children.length >= 2 && shownElementAmount > 1) {
      const navigationBar = toolbox.children[1] as HTMLElement;
      if (navigationBar?.style.display !== "") {
        navigationBar.style.display = "";
      }
    }
  } catch (e) {
    console.error("[ui-toggle] Failed to toggle user interface:", e);
  }
}

/**
 * Toggle visibility of the `#nav-bar` element.
 */
export function toggleNavigationPanel(doc: Document): void {
  try {
    const navBar = doc.getElementById("nav-bar") as HTMLElement | null;
    if (!navBar) return;
    navBar.style.display = navBar.style.display ? "" : "none";
  } catch (e) {
    console.error("[ui-toggle] Failed to toggle navigation panel:", e);
  }
}

/**
 * Enable rest mode: discard all tabs, show a blank screen with an alert
 * prompt, then restore the original tab when the user dismisses the dialog.
 */
export function enableRestMode(win: Window): void {
  try {
    const doc = win.document!;
    const selectedTab = win.gBrowser.selectedTab;
    const selectedTabLocation =
      selectedTab.linkedBrowser.documentURI?.spec;

    for (const tab of win.gBrowser.tabs) {
      win.gBrowser.discardBrowser(tab);
    }

    if (selectedTabLocation) {
      win.openTrustedLinkIn("about:blank", "current");
    }

    const tag = doc.createElement("style");
    tag.textContent = `* { display:none !important; }`;
    tag.setAttribute("id", "floorp-rest-mode");
    doc.head?.appendChild(tag);

    const l10n = new Localization(
      ["browser/floorp.ftl", "branding/brand.ftl"],
      true,
    );
    Services.prompt.alert(
      null as unknown as mozIDOMWindowProxy,
      l10n.formatValueSync("rest-mode") ?? "Rest Mode",
      l10n.formatValueSync("rest-mode-description") ?? "",
    );

    doc.getElementById("floorp-rest-mode")?.remove();

    if (selectedTabLocation) {
      win.openTrustedLinkIn(selectedTabLocation, "current");
    }
  } catch (e) {
    console.error("[ui-toggle] Failed to enable rest mode:", e);
  }
}
