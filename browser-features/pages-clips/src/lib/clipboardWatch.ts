import { clips } from "@/lib/rpc/rpc.ts";

/** How often clipboard-history mode looks at the clipboard. */
const POLL_MS = 1000;

/**
 * Watch the clipboard while the page is open.
 *
 * Whatever is already on the clipboard when watching starts is treated as
 * "seen", so opening the panel does not clip something copied long before.
 * Copying the same thing twice in a row is ignored, as the spec asks.
 *
 * Nothing here survives the page: stop watching and Clips stops looking.
 */
export function watchClipboard(onText: (text: string) => void): () => void {
  let last: string | null = null;
  let started = false;
  let alive = true;

  const tick = async () => {
    let text: string | null = null;
    try {
      text = await clips.readClipboardText();
    } catch (e) {
      console.error("[Floorp Clips] Could not read the clipboard:", e);
      return;
    }
    if (!alive) return;

    if (!started) {
      started = true;
      last = text;
      return;
    }
    if (text === null || text === last) return;
    last = text;
    onText(text);
  };

  void tick();
  const timer = setInterval(() => void tick(), POLL_MS);

  return () => {
    alive = false;
    clearInterval(timer);
  };
}
