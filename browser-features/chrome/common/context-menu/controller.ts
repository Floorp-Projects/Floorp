// SPDX-License-Identifier: MPL-2.0

import {
  ContextMenuCatalogBuilder,
  OptionalContextMenuCatalogReporter,
} from "./catalog.ts";
import {
  isContextMenuConfigEmpty,
  resolveContextMenuLevelOverride,
} from "./config.ts";
import {
  type ContextMenuConfigSnapshot,
  ContextMenuConfigStore,
} from "./config-store.ts";
import {
  ContextMenuRegistry,
  FLOORP_CONTEXT_MENU_KEY_ATTRIBUTE,
} from "./registry.ts";
import { ContextMenuTransaction } from "./transaction.ts";
import type { ContextMenuCatalogReporter } from "./types.ts";

export interface ContextMenuControllerOptions {
  window: Window;
  registry?: ContextMenuRegistry;
  configStore?: ContextMenuConfigStore;
  catalogBuilder?: ContextMenuCatalogBuilder;
  catalogReporter?: ContextMenuCatalogReporter;
  ownerId?: string;
  scheduleMicrotask?: (callback: () => void) => void;
}

interface ElementWithPopupState extends Element {
  readonly state?: string;
}

function isPopupActive(popup: Element): boolean {
  if (!popup.isConnected) return false;
  const state = (popup as ElementWithPopupState).state;
  return state === undefined || state === "showing" || state === "open";
}

function createOwnerId(document: Document): string {
  return `context-menu-window:${Services.uuid.generateUUID().toString()}:${document.documentURI}`;
}

function hasEffectiveChanges(snapshot: ContextMenuConfigSnapshot): boolean {
  return snapshot.enabled && !isContextMenuConfigEmpty(snapshot.config);
}

const NATIVE_MUTATION_ATTRIBUTES = [
  "hidden",
  "collapsed",
  "label",
  "data-l10n-id",
  "data-lazy-l10n-id",
  "id",
  "class",
  "generateditemid",
  "ext-type",
  FLOORP_CONTEXT_MENU_KEY_ATTRIBUTE,
];

export class ContextMenuController {
  readonly #window: Window;
  readonly #document: Document;
  readonly #registry: ContextMenuRegistry;
  readonly #configStore: ContextMenuConfigStore;
  readonly #catalogBuilder: ContextMenuCatalogBuilder;
  readonly #catalogReporter: ContextMenuCatalogReporter;
  readonly #ownerId: string;
  readonly #scheduleMicrotask: (callback: () => void) => void;
  readonly #transactions = new Map<Element, ContextMenuTransaction[]>();
  readonly #mutationObservers = new Map<Element, MutationObserver>();
  readonly #generations = new WeakMap<Element, number>();
  #unsubscribeConfig: (() => void) | null = null;
  #attached = false;
  #destroyed = false;

  readonly #onPopupShowing: EventListener = (event) => {
    const popup = this.getEventTarget(event);
    if (!popup) return;
    if (!this.#registry.resolvePopup(popup, this.#window)) return;

    // A stale transaction must never become input to Firefox's next builder.
    this.resetPopupTree(popup);
    this.scheduleReconcile(popup, event);
  };

  readonly #onPopupShown: EventListener = (event) => {
    const popup = this.getEventTarget(event);
    if (!popup) return;
    if (!this.#registry.resolvePopup(popup, this.#window)) return;

    // Some Firefox builders await asynchronous data during popupshowing. A
    // popupshown pass captures their final DOM and replaces the early overlay.
    this.scheduleReconcile(popup);
  };

  readonly #onPopupHiding: EventListener = (event) => {
    const popup = this.getEventTarget(event);
    if (!popup) return;
    this.resetPopupTree(popup);

    if (event.type === "popuphiding") {
      // Gecko can cancel popuphiding for non-native popups. Native handlers
      // still receive the original DOM because rollback happens in capture;
      // restore the overlay after dispatch when the close was cancelled.
      this.#scheduleMicrotask(() => {
        if (
          !this.#destroyed && event.defaultPrevented && isPopupActive(popup)
        ) {
          this.scheduleReconcile(popup);
        }
      });
    }
  };

  constructor(options: ContextMenuControllerOptions) {
    this.#window = options.window;
    this.#document = options.window.document;
    this.#registry = options.registry ?? new ContextMenuRegistry();
    this.#configStore = options.configStore ?? new ContextMenuConfigStore();
    this.#catalogBuilder = options.catalogBuilder ??
      new ContextMenuCatalogBuilder(this.#registry);
    this.#catalogReporter = options.catalogReporter ??
      new OptionalContextMenuCatalogReporter();
    this.#ownerId = options.ownerId ?? createOwnerId(this.#document);
    // `queueMicrotask` is a WebIDL Window method in Gecko. Storing the bare
    // function and later invoking it through this private field would bind the
    // controller as its receiver and throw before reconciliation can run.
    this.#scheduleMicrotask = options.scheduleMicrotask ??
      ((callback) => this.#window.queueMicrotask(callback));
  }

  attach(): void {
    if (this.#attached || this.#destroyed) return;
    this.#attached = true;
    this.#document.addEventListener(
      "popupshowing",
      this.#onPopupShowing,
      true,
    );
    this.#document.addEventListener(
      "popuphiding",
      this.#onPopupHiding,
      true,
    );
    this.#document.addEventListener(
      "popupshown",
      this.#onPopupShown,
      true,
    );
    this.#document.addEventListener(
      "popuphidden",
      this.#onPopupHiding,
      true,
    );
    this.#unsubscribeConfig = this.#configStore.subscribe(() => {
      // Never keep an overlay based on a stale preference snapshot.
      const openPopups = new Set([
        ...this.#mutationObservers.keys(),
        ...this.#transactions.keys(),
      ]);
      this.stopObservingAllPopups();
      this.rollbackAll();
      for (const popup of openPopups) {
        if (isPopupActive(popup)) this.scheduleReconcile(popup);
      }
    });
    this.#configStore.start();
    this.#catalogReporter.report(
      this.#ownerId,
      this.#catalogBuilder.snapshot(),
    );
  }

  destroy(): void {
    if (this.#destroyed) return;
    this.#destroyed = true;
    if (this.#attached) {
      this.#document.removeEventListener(
        "popupshowing",
        this.#onPopupShowing,
        true,
      );
      this.#document.removeEventListener(
        "popuphiding",
        this.#onPopupHiding,
        true,
      );
      this.#document.removeEventListener(
        "popupshown",
        this.#onPopupShown,
        true,
      );
      this.#document.removeEventListener(
        "popuphidden",
        this.#onPopupHiding,
        true,
      );
    }
    this.#attached = false;
    this.#unsubscribeConfig?.();
    this.#unsubscribeConfig = null;
    this.stopObservingAllPopups();
    this.rollbackAll();
    this.#configStore.destroy();
    this.#catalogReporter.removeOwner(this.#ownerId);
  }

  private getEventTarget(event: Event): Element | null {
    const target = event.target;
    if (!(target instanceof this.#window.Element)) return null;
    return target as Element;
  }

  private scheduleReconcile(popup: Element, openingEvent?: Event): void {
    const generation = (this.#generations.get(popup) ?? 0) + 1;
    this.#generations.set(popup, generation);

    this.#scheduleMicrotask(() => {
      if (this.#destroyed) return;
      this.pruneDisconnectedPopups();
      if (openingEvent?.defaultPrevented) return;
      if (this.#generations.get(popup) !== generation) return;
      if (!isPopupActive(popup)) return;

      // Always return to native DOM before observing or reapplying. This makes
      // popupshown reconciliation independent from the early microtask pass.
      this.stopObservingPopup(popup);
      let shouldObserve = false;
      try {
        this.rollbackPopup(popup);
        const surface = this.#registry.resolvePopup(popup, this.#window);
        if (!surface) return;
        shouldObserve = true;

        const catalog = this.#catalogBuilder.record(surface);
        this.#catalogReporter.report(this.#ownerId, catalog);

        const snapshot = this.#configStore.getSnapshot();
        if (!hasEffectiveChanges(snapshot)) return;

        const applied: ContextMenuTransaction[] = [];
        for (
          const container of [
            surface,
            ...this.#registry.resolveVirtualContainers(surface),
          ]
        ) {
          const containerLevel = resolveContextMenuLevelOverride(
            snapshot.config,
            container.adapter.key,
            container.profileKey,
            container.containerKey,
          );
          if (
            !containerLevel ||
            (containerLevel.hidden.length === 0 &&
              containerLevel.order.length === 0)
          ) {
            continue;
          }
          const transaction = new ContextMenuTransaction(
            container,
            this.#registry,
            containerLevel,
          );
          if (transaction.apply()) applied.push(transaction);
          else transaction.rollback();
        }
        if (applied.length > 0) this.#transactions.set(popup, applied);
      } finally {
        if (shouldObserve && !this.#destroyed && isPopupActive(popup)) {
          this.observePopup(popup);
        }
      }
    });
  }

  private observePopup(popup: Element): void {
    this.stopObservingPopup(popup);
    const observer = new this.#window.MutationObserver(
      (records: MutationRecord[]) => {
        if (
          !records.some((record) => this.isMutationInPopupLevel(popup, record))
        ) {
          return;
        }
        if (this.#destroyed || !isPopupActive(popup)) {
          this.stopObservingPopup(popup);
          this.rollbackPopup(popup);
          return;
        }
        this.stopObservingPopup(popup);
        this.rollbackPopup(popup);
        this.scheduleReconcile(popup);
      },
    );
    observer.observe(popup, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: NATIVE_MUTATION_ATTRIBUTES,
    });
    this.#mutationObservers.set(popup, observer);
  }

  private isMutationInPopupLevel(
    popup: Element,
    record: MutationRecord,
  ): boolean {
    if (record.target.nodeType !== 1) return false;
    let current: Element | null = record.target as Element;
    while (current && current !== popup) {
      // A child menupopup has its own popup lifecycle and observer. The parent
      // still observes menugroup descendants, which are virtual containers.
      if (current.localName === "menupopup") return false;
      current = current.parentElement;
    }
    return current === popup;
  }

  private stopObservingPopup(popup: Element): void {
    this.#mutationObservers.get(popup)?.disconnect();
    this.#mutationObservers.delete(popup);
  }

  private stopObservingAllPopups(): void {
    for (const observer of this.#mutationObservers.values()) {
      observer.disconnect();
    }
    this.#mutationObservers.clear();
  }

  private resetPopupTree(rootPopup: Element): void {
    this.pruneDisconnectedPopups();
    const popups = new Set([
      rootPopup,
      ...this.#mutationObservers.keys(),
      ...this.#transactions.keys(),
    ]);
    for (const popup of popups) {
      if (popup !== rootPopup && !rootPopup.contains(popup)) continue;
      this.#generations.set(popup, (this.#generations.get(popup) ?? 0) + 1);
      this.stopObservingPopup(popup);
      this.rollbackPopup(popup);
    }
  }

  private pruneDisconnectedPopups(): void {
    const popups = new Set([
      ...this.#mutationObservers.keys(),
      ...this.#transactions.keys(),
    ]);
    for (const popup of popups) {
      if (popup.isConnected) continue;
      this.#generations.set(popup, (this.#generations.get(popup) ?? 0) + 1);
      this.stopObservingPopup(popup);
      this.rollbackPopup(popup);
    }
  }

  private rollbackPopup(popup: Element): void {
    const transactions = this.#transactions.get(popup);
    if (!transactions) return;
    for (let index = transactions.length - 1; index >= 0; index--) {
      transactions[index].rollback();
    }
    this.#transactions.delete(popup);
  }

  private rollbackAll(): void {
    for (const transactions of this.#transactions.values()) {
      for (let index = transactions.length - 1; index >= 0; index--) {
        transactions[index].rollback();
      }
    }
    this.#transactions.clear();
  }
}
