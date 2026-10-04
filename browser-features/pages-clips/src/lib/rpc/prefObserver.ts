import type {
  ObservedPref,
  PrefReaders,
  PrefValue,
} from "../../types/prefObserver.ts";

/** Development polling uses the preference's actual type, just like Gecko. */
export function createPrefPoller(
  pref: ObservedPref,
  readers: PrefReaders,
  onChanged: (name: string) => void,
): { poll: () => Promise<void>; stop: () => void } {
  let last: PrefValue = null;
  let initialized = false;
  let active = true;
  let reading = false;

  const poll = async () => {
    if (!active || reading) return;
    reading = true;
    try {
      const current = pref.kind === "int"
        ? await readers.getIntPref(pref.name)
        : pref.kind === "bool"
        ? await readers.getBoolPref(pref.name)
        : await readers.getStringPref(pref.name);
      if (!active) return;
      if (!initialized) {
        initialized = true;
        last = current;
      } else if (current !== last) {
        last = current;
        onChanged(pref.name);
      }
    } catch {
      // A missing pref or a temporarily unavailable actor is retried later.
    } finally {
      reading = false;
    }
  };

  return { poll, stop: () => (active = false) };
}
