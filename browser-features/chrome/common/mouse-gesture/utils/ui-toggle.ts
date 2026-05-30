// SPDX-License-Identifier: MPL-2.0
// Shared UI toggle utilities for keyboard shortcuts and mouse gestures.
// Extracted from legacy gFloorpDesign (Floorp-core/browser/base/content/browser-design.mjs).

/**
 * Toggle visibility of all `#navigator-toolbox` children.
 * Computes a single hide/show target state from the children's current
 * visibility, then applies it consistently to all children to avoid mixed
 * UI states. When restoring (showing), ensures the navigation bar remains
 * visible if multiple elements are shown.
 */
export function toggleUserInterface(doc: Document): void {
  try {
    const toolbox = doc.getElementById("navigator-toolbox");
    if (!toolbox) return;
    if (toolbox.children.length === 0) return;

    // Compute a single target state: if any child is shown, hide all.
    // Otherwise, show all. This avoids mixed UI states.
    let anyShown = false;
    for (const child of toolbox.children) {
      const el = child as HTMLElement;
      if (el.style.display !== "none") {
        anyShown = true;
        break;
      }
    }
    const targetDisplay = anyShown ? "none" : "";

    for (const child of toolbox.children) {
      (child as HTMLElement).style.display = targetDisplay;
    }

    // When restoring, ensure the navigation bar stays visible.
    if (!anyShown && toolbox.children.length >= 2) {
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
  const doc = win.document!;
  const selectedTab = win.gBrowser.selectedTab;
  const selectedTabLocation =
    selectedTab.linkedBrowser.documentURI?.spec;
  const tag = doc.createElement("style");

  try {
    for (const tab of win.gBrowser.tabs) {
      win.gBrowser.discardBrowser(tab);
    }

    if (selectedTabLocation) {
      win.openTrustedLinkIn("about:blank", "current");
    }

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
  } catch (e) {
    console.error("[ui-toggle] Failed to enable rest mode:", e);
  } finally {
    tag.remove();
    if (selectedTabLocation) {
      win.openTrustedLinkIn(selectedTabLocation, "current");
    }
  }
}
