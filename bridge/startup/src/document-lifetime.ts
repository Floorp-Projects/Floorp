// SPDX-License-Identifier: MPL-2.0
/** Own a strong observer subscription and DOM work for one document only. */
export function bindDocumentLifetime(
  doc: EventTarget & { readyState: string; defaultView: EventTarget | null },
  initialize: () => void,
  subscribe: () => () => void,
  disconnect: () => void,
  report: (error: unknown) => void,
): () => void {
  let disposed = false;
  let unsubscribe: (() => void) | undefined;
  const run = () => {
    if (!disposed) initialize();
  };
  const dispose = () => {
    if (disposed) return;
    disposed = true;
    doc.removeEventListener("DOMContentLoaded", run);
    doc.defaultView?.removeEventListener("unload", dispose);
    try {
      unsubscribe?.();
    } catch (error) {
      report(error);
    } finally {
      unsubscribe = undefined;
      disconnect();
    }
  };
  if (!doc.defaultView) return dispose;
  doc.defaultView.addEventListener("unload", dispose, { once: true });
  try {
    unsubscribe = subscribe();
  } catch (error) {
    report(error);
  }
  if (doc.readyState === "loading") {
    doc.addEventListener("DOMContentLoaded", run, { once: true });
  } else run();
  return dispose;
}
