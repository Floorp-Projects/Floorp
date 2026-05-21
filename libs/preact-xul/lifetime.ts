// SPDX-License-Identifier: MPL-2.0
//
// Stage 2 shim: Preact+signals equivalent of solid-xul's lifecycle utilities.
//
// Two functions only (ミオ + ムゥ判定):
//   1. createRootHMR — HMR lifetime morphism injection
//   2. createNodeDisposer — DOM lifetime → effect dispose 連動 (browser-action.tsx用)
//
// NOT implemented (自然変換成立で不要):
//   createSignal / createEffect / createMemo / onCleanup — 直書き換え
//   For / Show / Portal — preact native で代替
//   Scope / Owner / runWithOwner / getOwner — 全 callsite が直書き換え可 (Stage 1 判定)

import type { ViteHotContext } from "vite/types/hot.js";
import { effect } from "@preact/signals";

type Disposer = () => void;

// Context stack for tracking disposers inside createRootHMR fn
let _currentDisposers: Disposer[] | null = null;

/**
 * Register a disposer to be called on HMR reload.
 * Call inside createRootHMR's fn — typically wraps each effect() call.
 */
export function addDisposer(d: Disposer): void {
  _currentDisposers?.push(d);
}

/**
 * Preact+signals equivalent of solid-xul's createRootHMR.
 *
 * - Runs fn() synchronously (matching Solid's createRoot behavior)
 * - Collects disposers registered via addDisposer() or rootEffect() inside fn
 * - On HMR reload (hot.dispose), calls all collected disposers
 *
 * Usage:
 *   createRootHMR(() => {
 *     rootEffect(() => { ... });  // ← HMR-tracked effect
 *     addDisposer(() => cleanup());  // ← manual disposer
 *   }, import.meta.hot);
 */
export function createRootHMR<T>(fn: () => T, hot?: ViteHotContext): T {
  const disposers: Disposer[] = [];
  const prev = _currentDisposers;
  _currentDisposers = disposers;
  let result: T;
  try {
    result = fn();
  } finally {
    _currentDisposers = prev;
  }
  if (hot) {
    hot.dispose(() => {
      disposers.forEach((d) => d());
      disposers.length = 0;
    });
  }
  return result!;
}

/**
 * HMR-tracked effect. Wraps @preact/signals effect() and registers
 * the disposer with the current createRootHMR scope.
 *
 * Use this instead of bare effect() inside createRootHMR fn:
 *   createRootHMR(() => {
 *     rootEffect(() => { ... });  // ← disposer auto-registered
 *   }, import.meta.hot);
 */
export function rootEffect(fn: Parameters<typeof effect>[0]): Disposer {
  const d = effect(fn);
  addDisposer(d);
  return d;
}

/**
 * Attaches a disposer to a DOM node's lifecycle via MutationObserver.
 *
 * Needed for browser-action.tsx: CustomizableUI.createWidget passes an onCreated
 * callback that fires outside any reactive scope. We cannot getOwner() there.
 * Instead, bind the effect disposer to the node — when Chromium removes the node,
 * the effect is disposed automatically.
 *
 * @param node - The XUL/DOM node whose removal should trigger dispose
 * @param dispose - Disposer to call when node is removed from DOM
 */
export function createNodeDisposer(node: Element, dispose: Disposer): void {
  // Check if already disconnected
  if (!node.isConnected) {
    dispose();
    return;
  }

  const observer = new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      for (const removed of mutation.removedNodes) {
        if (removed === node || (removed as Element).contains?.(node)) {
          dispose();
          observer.disconnect();
          return;
        }
      }
    }
  });

  // Observe the closest parent that won't be removed itself
  const parent = node.parentElement ?? (document?.body as Element | null);
  if (!parent) {
    console.warn("createNodeDisposer: no observable parent, disposer leaked");
    return;
  }
  observer.observe(parent, { childList: true, subtree: true });
}
