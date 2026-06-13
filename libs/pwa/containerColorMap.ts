/**
 * Maps Firefox Contextual Identity color values to CSS variable suffixes.
 * Shared between chrome UI and parent-process icon badging.
 */

export const CONTAINER_COLOR_NUMBER_MAP: Record<number, string> = {
  0: "blue",
  1: "turquoise",
  2: "green",
  3: "yellow",
  4: "orange",
  5: "red",
  6: "pink",
  7: "purple",
  8: "toolbar",
  9: "gray",
};

/** Fallback hex when theme CSS variables are unavailable (e.g. headless). */
export const CONTAINER_COLOR_HEX: Record<string, string> = {
  blue: "#37adff",
  turquoise: "#00c79a",
  green: "#51cd00",
  yellow: "#ffcb00",
  orange: "#ff9f00",
  red: "#ff613d",
  pink: "#ff4bda",
  purple: "#af51f5",
  toolbar: "#ffffff",
  gray: "#909090",
};

const VALID_COLOR_NAMES = new Set(Object.keys(CONTAINER_COLOR_HEX));

export function mapColorToCSSVariable(
  color: number | string | null | undefined,
): string | null {
  if (color === null || color === undefined) {
    return null;
  }

  if (typeof color === "string") {
    const normalizedColor = color.toLowerCase();
    if (normalizedColor === "white") {
      return "toolbar";
    }
    if (normalizedColor === "grey") {
      return "gray";
    }
    if (VALID_COLOR_NAMES.has(normalizedColor)) {
      return normalizedColor;
    }
    return null;
  }

  return CONTAINER_COLOR_NUMBER_MAP[color] ?? null;
}

export function getContainerColorHex(colorName: string): string {
  return CONTAINER_COLOR_HEX[colorName] ?? CONTAINER_COLOR_HEX.blue;
}

const IDENTITY_COLOR_CSS_VARS = [
  (name: string) => `--identity-color-${name}`,
  (name: string) => `--identity-tab-color-${name}`,
] as const;

/**
 * Resolves a container color name to a concrete CSS color value.
 * Theme variables may be unset in PWA windows; falls back to hex map.
 */
type ColorResolvableDocument = {
  documentElement: unknown;
  defaultView: {
    getComputedStyle: (element: unknown) => {
      getPropertyValue: (property: string) => string;
    };
  } | null;
};

export function resolveContainerDisplayColor(
  colorName: string | null | undefined,
  doc: ColorResolvableDocument | null = null,
): string {
  const resolvedDoc = doc ??
    ("document" in globalThis
      ? (globalThis as { document?: ColorResolvableDocument }).document ?? null
      : null);
  const name = colorName && VALID_COLOR_NAMES.has(colorName) ? colorName : "blue";
  const root = resolvedDoc?.documentElement;
  const view = resolvedDoc?.defaultView;
  if (root && view) {
    const style = view.getComputedStyle(root);
    for (const toVar of IDENTITY_COLOR_CSS_VARS) {
      const value = style.getPropertyValue(toVar(name)).trim();
      if (value) {
        return value;
      }
    }
  }
  return getContainerColorHex(name);
}
