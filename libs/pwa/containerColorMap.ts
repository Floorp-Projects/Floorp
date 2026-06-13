/**
 * Maps Firefox Contextual Identity color values to CSS variable suffixes.
 * Shared between chrome UI and parent-process icon badging.
 *
 * Canonical color values live in Firefox's usercontext.css:
 * browser/components/contextualidentity/content/usercontext.css
 * (classes `.identity-color-{name}` define `--identity-tab-color`).
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

/**
 * Last-resort fallback when usercontext.css is unavailable (headless, tests).
 * Values mirror usercontext.css defaults.
 */
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

const IDENTITY_COLOR_PROPERTIES_ON_CLASS = [
  "--identity-tab-color",
  "--identity-icon-color",
] as const;

const IDENTITY_COLOR_CSS_VARS_ON_ROOT = [
  (name: string) => `--identity-color-${name}`,
  (name: string) => `--identity-tab-color-${name}`,
] as const;

type ColorResolvableWindow = {
  document: {
    documentElement: {
      appendChild: (node: unknown) => void;
    };
    createElement: (tag: string) => {
      className: string;
      hidden: boolean;
      remove: () => void;
    };
  };
  getComputedStyle: (element: unknown) => {
    getPropertyValue: (property: string) => string;
  };
};

type ColorResolvableDocument = {
  documentElement: unknown;
  defaultView: ColorResolvableWindow | null;
};

function normalizeColorName(
  colorName: string | null | undefined,
): string {
  return colorName && VALID_COLOR_NAMES.has(colorName) ? colorName : "blue";
}

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

/**
 * Reads the resolved color from Firefox's usercontext.css via a probe element
 * with class `identity-color-{name}`.
 */
export function resolveContainerDisplayColorFromWindow(
  colorName: string | null | undefined,
  win: ColorResolvableWindow,
): string {
  const name = normalizeColorName(colorName);
  const doc = win.document;
  const probe = doc.createElement("span");
  probe.className = `identity-color-${name}`;
  probe.hidden = true;
  doc.documentElement.appendChild(probe);

  try {
    const probeStyle = win.getComputedStyle(probe);
    for (const prop of IDENTITY_COLOR_PROPERTIES_ON_CLASS) {
      const value = probeStyle.getPropertyValue(prop).trim();
      if (value) {
        return value;
      }
    }

    const rootStyle = win.getComputedStyle(doc.documentElement);
    for (const toVar of IDENTITY_COLOR_CSS_VARS_ON_ROOT) {
      const value = rootStyle.getPropertyValue(toVar(name)).trim();
      if (value) {
        return value;
      }
    }
  } finally {
    probe.remove();
  }

  return getContainerColorHex(name);
}

/**
 * Resolves a container color name to a concrete CSS color value.
 */
export function resolveContainerDisplayColor(
  colorName: string | null | undefined,
  doc: ColorResolvableDocument | null = null,
): string {
  const resolvedDoc = doc ??
    ("document" in globalThis
      ? (globalThis as { document?: ColorResolvableDocument }).document ?? null
      : null);
  const view = resolvedDoc?.defaultView;
  if (view) {
    return resolveContainerDisplayColorFromWindow(colorName, view);
  }
  return getContainerColorHex(normalizeColorName(colorName));
}
