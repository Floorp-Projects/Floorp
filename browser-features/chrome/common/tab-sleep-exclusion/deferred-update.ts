// SPDX-License-Identifier: MPL-2.0

/** Own delayed tab updates so they cannot run after component cleanup. */
export function createDeferredTabUpdater<T>(update: (tab: T) => void) {
  const pending = new Set<ReturnType<typeof setTimeout>>();
  let disposed = false;
  return {
    schedule(tab: T): void {
      if (disposed) return;
      const timer = setTimeout(() => {
        pending.delete(timer);
        if (!disposed) update(tab);
      }, 100);
      pending.add(timer);
    },
    dispose(): void {
      disposed = true;
      for (const timer of pending) clearTimeout(timer);
      pending.clear();
    },
  };
}
