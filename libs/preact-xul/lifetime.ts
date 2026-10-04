// SPDX-License-Identifier: MPL-2.0
import type { ViteHotContext } from "vite/types/hot.js";
import { effect } from "@preact/signals";

type Disposer = () => void;
type Scope = { disposers: Disposer[]; disposed: boolean };
let currentScope: Scope | null = null;
const hotRoots = new WeakMap<ViteHotContext, Set<Disposer>>();

/** Register synchronous setup cleanup with the active root, if there is one. */
export function addDisposer(dispose: Disposer): void {
  if (currentScope?.disposed) dispose();
  else currentScope?.disposers.push(dispose);
}

function disposeAll(disposers: Disposer[]): void {
  for (const dispose of disposers.splice(0).reverse()) {
    try {
      dispose();
    } catch (error) {
      // A failing feature cleanup must not leak every remaining observer/root.
      console.error("[@nora/preact-xul] cleanup failed", error);
    }
  }
}

/** Synchronous lifetime scope for effects, observers and inserted render roots. */
export function createRoot<T>(fn: (dispose: Disposer) => T): T {
  const scope: Scope = { disposers: [], disposed: false };
  const dispose = () => {
    if (scope.disposed) return;
    scope.disposed = true;
    disposeAll(scope.disposers);
  };
  const parent = currentScope;
  addDisposer(dispose);
  currentScope = scope;
  try {
    return fn(dispose);
  } catch (error) {
    dispose();
    throw error;
  } finally {
    currentScope = parent;
  }
}

/** Drain all roots for one module; NoraComponentBase can own its HMR callback. */
export function disposeRoot(hot: ViteHotContext): void {
  const roots = hotRoots.get(hot);
  hotRoots.delete(hot);
  if (roots) disposeAll(Array.from(roots));
}

export function createRootHMR<T>(
  fn: (dispose: Disposer) => T,
  hot?: ViteHotContext,
): T {
  return createRoot((dispose) => {
    if (hot) {
      let roots = hotRoots.get(hot);
      if (!roots) hotRoots.set(hot, roots = new Set());
      roots.add(dispose);
      addDisposer(() => roots.delete(dispose));
      // Vite stores one dispose callback per module. Register once so multiple
      // roots do not overwrite one another or NoraComponentBase's callback.
      if (
        !hot.data.__preactXulExternalDisposeOwner &&
        !hot.data.__preactXulDisposeRegistered
      ) {
        hot.data.__preactXulDisposeRegistered = true;
        hot.dispose(() => {
          try {
            disposeRoot(hot);
          } finally {
            hot.data.__preactXulDisposeRegistered = false;
          }
        });
      }
    }
    return fn(dispose);
  });
}

/** Track the effect's subscription and its returned cleanup with this root. */
export function rootEffect(fn: Parameters<typeof effect>[0]): Disposer {
  const dispose = effect(fn);
  addDisposer(dispose);
  return dispose;
}

/**
 * Dispose after a node (or an ancestor) leaves its document. CustomizableUI calls
 * onCreated before insertion, and moves widgets between toolbars: wait for an
 * initial attachment and inspect connectivity after each mutation batch.
 */
export function createNodeDisposer(node: Element, cleanup: Disposer): Disposer {
  const doc = node.ownerDocument;
  const view = doc.defaultView;
  let connected = node.isConnected;
  let disposed = false;
  const dispose = () => {
    if (disposed) return;
    disposed = true;
    observer.disconnect();
    view?.removeEventListener("unload", dispose);
    cleanup();
  };
  const observer = new MutationObserver((mutations) => {
    if (node.isConnected) {
      connected = true;
      return;
    }
    const removed = mutations.some((mutation) =>
      Array.from(mutation.removedNodes).some((ancestor) =>
        ancestor?.contains(node)
      )
    );
    if (connected || removed) dispose();
  });
  observer.observe(doc, { childList: true, subtree: true });
  view?.addEventListener("unload", dispose, { once: true });
  addDisposer(dispose);
  return dispose;
}
