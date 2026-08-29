// SPDX-License-Identifier: MPL-2.0

// Keep this as a relative runtime import. loader-modules bundles relative
// dependencies into the packaged actor, while browser-feature aliases are only
// understood by the development feature loader and would remain bare in the
// resource:// artifact consumed by Gecko.
import { ContextMenuController } from "../../chrome/common/context-menu/controller.ts";

const SECONDARY_CONTEXT_MENU_DOCUMENTS = new Set([
  "chrome://browser/content/places/places.xhtml",
  "chrome://browser/content/places/bookmarksSidebar.xhtml",
  "chrome://browser/content/places/historySidebar.xhtml",
]);

export function isSecondaryContextMenuDocumentUri(uri: string): boolean {
  const normalized = uri.split(/[?#]/, 1)[0];
  return SECONDARY_CONTEXT_MENU_DOCUMENTS.has(normalized);
}

/** Runs the shared customizer in chrome documents outside browser.xhtml. */
export class NRContextMenuChild extends JSWindowActorChild {
  #controller: ContextMenuController | null = null;

  actorCreated(): void {
    this.attachIfSupported();
  }

  handleEvent(_event: Event): void {
    this.attachIfSupported();
  }

  didDestroy(): void {
    this.#controller?.destroy();
    this.#controller = null;
  }

  private attachIfSupported(): void {
    if (this.#controller) return;
    const targetWindow = this.contentWindow;
    const targetDocument = targetWindow?.document;
    if (
      !targetWindow ||
      !targetDocument ||
      !isSecondaryContextMenuDocumentUri(targetDocument.documentURI)
    ) {
      return;
    }

    this.#controller = new ContextMenuController({
      window: targetWindow as unknown as Window,
    });
    this.#controller.attach();
  }
}
