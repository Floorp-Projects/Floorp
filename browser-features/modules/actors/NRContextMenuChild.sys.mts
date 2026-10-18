// SPDX-License-Identifier: MPL-2.0

// Keep this as a relative runtime import. loader-modules bundles relative
// dependencies into the packaged actor, while browser-feature aliases are only
// understood by the development feature loader and would remain bare in the
// resource:// artifact consumed by Gecko.
import { ContextMenuRuntime } from "../../chrome/common/context-menu/runtime.ts";

const SECONDARY_CONTEXT_MENU_DOCUMENTS = new Set([
  "chrome://browser/content/places/places.xhtml",
  "chrome://browser/content/places/bookmarksSidebar.xhtml",
  "chrome://browser/content/places/historySidebar.xhtml",
  "chrome://browser/content/webext-panels.xhtml",
]);

export function isSecondaryContextMenuDocumentUri(uri: string): boolean {
  const normalized = uri.split(/[?#]/, 1)[0];
  return SECONDARY_CONTEXT_MENU_DOCUMENTS.has(normalized);
}

/** Runs the shared customizer in chrome documents outside browser.xhtml. */
export class NRContextMenuChild extends JSWindowActorChild {
  #runtime: ContextMenuRuntime | null = null;

  actorCreated(): void {
    this.attachIfSupported();
  }

  handleEvent(_event: Event): void {
    this.attachIfSupported();
  }

  didDestroy(): void {
    this.#runtime?.destroy();
    this.#runtime = null;
  }

  private attachIfSupported(): void {
    if (this.#runtime) return;
    const targetWindow = this.contentWindow;
    const targetDocument = targetWindow?.document;
    if (
      !targetWindow ||
      !targetDocument ||
      !isSecondaryContextMenuDocumentUri(targetDocument.documentURI)
    ) {
      return;
    }

    this.#runtime = new ContextMenuRuntime({
      window: targetWindow as unknown as Window,
    });
    this.#runtime.start();
  }
}
