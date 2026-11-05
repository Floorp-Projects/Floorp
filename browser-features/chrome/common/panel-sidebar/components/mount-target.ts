// SPDX-License-Identifier: MPL-2.0

/** Resolve mount targets only after this window's document and sidebar are ready. */
export async function getPanelMountTargets(
  doc: Document,
  sidebarReady: Promise<void>,
  win: Window,
): Promise<{ parent: HTMLElement; before: HTMLElement } | null> {
  if (win.closed) return null;
  let onDocumentReady: (() => void) | undefined;
  let onUnload: (() => void) | undefined;
  const documentReady = doc.readyState === "loading"
    ? new Promise<void>((resolve) => {
      onDocumentReady = resolve;
      doc.addEventListener("DOMContentLoaded", onDocumentReady, { once: true });
    })
    : Promise.resolve();
  const unloaded = new Promise<false>((resolve) => {
    onUnload = () => resolve(false);
    win.addEventListener("unload", onUnload, { once: true });
  });
  let ready: boolean;
  try {
    // Attach a rejection handler to sidebarReady immediately, even while DOM
    // parsing is pending. Do not keep waiting when this window unloads.
    ready = await Promise.race([
      Promise.all([documentReady, sidebarReady]).then(() => true),
      unloaded,
    ]);
  } finally {
    if (onDocumentReady) {
      doc.removeEventListener("DOMContentLoaded", onDocumentReady);
    }
    if (onUnload) win.removeEventListener("unload", onUnload);
  }
  if (!ready || win.closed) return null;
  const parent = doc.getElementById("browser");
  const before = doc.getElementById("tabbrowser-tabbox");
  if (!parent || !before || before.parentNode !== parent) {
    throw new Error("Panel sidebar mount targets are not ready");
  }
  return { parent, before };
}
