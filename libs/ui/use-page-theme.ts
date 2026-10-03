import { useLayoutEffect, useState } from "react";
import type { PageTheme, ResolvedPageTheme } from "./types.ts";

export function usePageTheme(theme: PageTheme): ResolvedPageTheme {
  const [systemTheme, setSystemTheme] = useState<ResolvedPageTheme>(() =>
    globalThis.matchMedia("(prefers-color-scheme: dark)").matches
      ? "dark"
      : "light"
  );
  const resolved = theme === "system" ? systemTheme : theme;

  useLayoutEffect(() => {
    const media = globalThis.matchMedia("(prefers-color-scheme: dark)");
    const update = () => setSystemTheme(media.matches ? "dark" : "light");
    update();
    media.addEventListener("change", update);
    return () => media.removeEventListener("change", update);
  }, []);

  useLayoutEffect(() => {
    const root = document.documentElement;
    root.classList.toggle("dark", resolved === "dark");
    root.classList.toggle("light", resolved === "light");
    root.dataset.theme = resolved;
  }, [resolved]);
  return resolved;
}
