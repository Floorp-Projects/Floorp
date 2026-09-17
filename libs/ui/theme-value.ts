import type { PageTheme } from "./types.ts";

// Gecko PreferenceSheet.cpp: 0 = dark, 1 = light, other = theme-derived.
export function themeFromPreference(value: number | null): PageTheme {
  return value === 0 ? "dark" : value === 1 ? "light" : "system";
}

export function themeToPreference(theme: PageTheme): number {
  return theme === "dark" ? 0 : theme === "light" ? 1 : 2;
}
