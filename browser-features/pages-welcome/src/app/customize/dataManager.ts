import type { SearchEngine } from "./types.ts";
import { rpc } from "../../lib/rpc/rpc";

function withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
  return Promise.race([
    promise,
    new Promise<T>((_, reject) =>
      setTimeout(() => reject(new Error(`Timed out after ${ms}ms`)), ms)
    ),
  ]);
}

function callNRFunction<T>(
  fn: ((callback: (data: string) => void) => void) | undefined,
  name: string,
): Promise<T> {
  if (typeof fn !== "undefined") {
    return withTimeout(
      new Promise<T>((resolve, reject) => {
        fn((data: string) => {
          try {
            const parsed = JSON.parse(data);
            // Check for error responses from the actor
            if (parsed && typeof parsed === "object" && parsed.success === false) {
              reject(new Error(parsed.error || `${name} returned an error`));
            } else {
              resolve(parsed);
            }
          } catch (_e) {
            reject(new Error(`${name}: invalid JSON response`));
          }
        });
      }),
      5000,
    );
  }
  return Promise.reject(new Error(`${name} is not available`));
}

export function getSearchEngines(): Promise<SearchEngine[]> {
  // deno-lint-ignore no-window
  return callNRFunction<SearchEngine[]>(window.NRGetSearchEngines, "NRGetSearchEngines");
}

export function getDefaultEngine(): Promise<SearchEngine> {
  // deno-lint-ignore no-window
  return callNRFunction<SearchEngine>(window.NRGetDefaultEngine, "NRGetDefaultEngine");
}
export function setDefaultEngine(
  engineId: string,
): Promise<{ success: boolean; engineId: string }> {
  console.log("setDefaultEngine", engineId);
  // deno-lint-ignore no-window
  return callNRFunction((cb) => window.NRSetDefaultEngine(engineId, cb), "NRSetDefaultEngine");
}

export function getDefaultPrivateEngine(): Promise<SearchEngine> {
  // deno-lint-ignore no-window
  return callNRFunction<SearchEngine>(window.NRGetDefaultPrivateEngine, "NRGetDefaultPrivateEngine");
}

export function setDefaultPrivateEngine(
  engineId: string,
): Promise<{ success: boolean; engineId: string }> {
  // deno-lint-ignore no-window
  return callNRFunction((cb) => window.NRSetDefaultPrivateEngine(engineId, cb), "NRSetDefaultPrivateEngine");
}

export async function getThemeSetting(): Promise<number | null> {
  return await rpc.getIntPref(
    "layout.css.prefers-color-scheme.content-override",
  );
}

export async function setThemeSetting(
  theme: "system" | "light" | "dark",
): Promise<void> {
  const themeValue = theme === "light" ? 1 : theme === "dark" ? 0 : 2;
  await rpc.setIntPref(
    "layout.css.prefers-color-scheme.content-override",
    themeValue,
  );
}
