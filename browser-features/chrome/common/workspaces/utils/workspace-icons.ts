/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspace } from "./type.ts";

type ModuleStrings = Record<string, string>;

export const WORKSPACE_ICON_CANONICAL_PREFIX = "floorp-icon:v1:" as const;

/**
 * Form value used by the workspace modal to signal "keep the current icon".
 * A pure constant (no modal/component imports) so pages-layer tests can
 * import it without pulling in chrome components.
 */
export const WORKSPACE_ICON_NO_CHANGE_SENTINEL =
  "__floorp_workspace_icon_picker_no_change__";

export interface WorkspaceIconRegistryEntry {
  readonly slug: string;
  readonly canonicalId: `${typeof WORKSPACE_ICON_CANONICAL_PREFIX}${string}`;
  readonly alias: string;
  readonly assetPath: `../icons/${string}.svg`;
  readonly labelKey: `workspaces.icons.${string}`;
  readonly keywords: readonly string[];
}

const defineWorkspaceIcon = (
  slug: string,
  keywords: readonly string[],
): WorkspaceIconRegistryEntry => ({
  slug,
  canonicalId: `${WORKSPACE_ICON_CANONICAL_PREFIX}${slug}`,
  alias: slug,
  assetPath: `../icons/${slug}.svg`,
  labelKey: `workspaces.icons.${slug}`,
  keywords: [slug, ...keywords],
});

export const WORKSPACE_ICON_REGISTRY: readonly WorkspaceIconRegistryEntry[] =
  Object.freeze([
    defineWorkspaceIcon("article", ["document", "news", "read"]),
    defineWorkspaceIcon("book", ["library", "reading", "study"]),
    defineWorkspaceIcon("briefcase", ["business", "job", "work"]),
    defineWorkspaceIcon("cart", ["buy", "shop", "shopping"]),
    defineWorkspaceIcon("chat", ["conversation", "message", "talk"]),
    defineWorkspaceIcon("chill", ["break", "relax", "rest"]),
    defineWorkspaceIcon("circle", ["dot", "round", "simple"]),
    defineWorkspaceIcon("compass", ["direction", "explore", "travel"]),
    defineWorkspaceIcon("code", ["developer", "programming", "software"]),
    defineWorkspaceIcon("dollar", ["finance", "money", "payment"]),
    defineWorkspaceIcon("fence", ["boundary", "garden", "outdoor"]),
    defineWorkspaceIcon("fingerprint", ["default", "identity", "security"]),
    defineWorkspaceIcon("food", ["eat", "meal", "restaurant"]),
    defineWorkspaceIcon("fruit", ["apple", "fresh", "healthy"]),
    defineWorkspaceIcon("game", ["gaming", "play", "controller"]),
    defineWorkspaceIcon("gear", ["configuration", "settings", "tools"]),
    defineWorkspaceIcon("gift", ["birthday", "present", "surprise"]),
    defineWorkspaceIcon("key", ["access", "lock", "password"]),
    defineWorkspaceIcon("lightning", ["energy", "fast", "power"]),
    defineWorkspaceIcon("network", ["connection", "internet", "nodes"]),
    defineWorkspaceIcon("notes", ["memo", "notebook", "write"]),
    defineWorkspaceIcon("paint", ["art", "color", "creative"]),
    defineWorkspaceIcon("photo", ["camera", "image", "picture"]),
    defineWorkspaceIcon("pin", ["location", "map", "place"]),
    defineWorkspaceIcon("pet", ["animal", "cat", "dog"]),
    defineWorkspaceIcon("question", ["help", "support", "unknown"]),
    defineWorkspaceIcon("smartphone", ["device", "mobile", "phone"]),
    defineWorkspaceIcon("star", ["favorite", "featured", "special"]),
    defineWorkspaceIcon("tree", ["forest", "nature", "plant"]),
    defineWorkspaceIcon("vacation", ["holiday", "trip", "travel"]),
    defineWorkspaceIcon("love", ["heart", "like", "romance"]),
    defineWorkspaceIcon("moon", ["dark", "night", "sleep"]),
    defineWorkspaceIcon("music", ["audio", "song", "sound"]),
    defineWorkspaceIcon("user", ["account", "person", "profile"]),
  ]);

const FALLBACK_ENTRY = WORKSPACE_ICON_REGISTRY.find(
  (entry) => entry.slug === "fingerprint",
)!;
const ENTRY_BY_ALIAS: ReadonlyMap<string, WorkspaceIconRegistryEntry> = new Map(
  WORKSPACE_ICON_REGISTRY.map((entry) => [entry.alias, entry] as const),
);
const ENTRY_BY_CANONICAL_ID: ReadonlyMap<string, WorkspaceIconRegistryEntry> =
  new Map(
    WORKSPACE_ICON_REGISTRY.map((entry) => [entry.canonicalId, entry] as const),
  );

const getExactRegistryEntry = (
  value: unknown,
): WorkspaceIconRegistryEntry | undefined => {
  if (typeof value !== "string") {
    return undefined;
  }
  return ENTRY_BY_CANONICAL_ID.get(value) ?? ENTRY_BY_ALIAS.get(value);
};

export const isCanonicalWorkspaceIconId = (
  value: unknown,
): value is WorkspaceIconRegistryEntry["canonicalId"] =>
  typeof value === "string" && ENTRY_BY_CANONICAL_ID.has(value);

export const applyWorkspaceIconSelection = (
  workspace: TWorkspace,
  selection: unknown,
): TWorkspace =>
  isCanonicalWorkspaceIconId(selection)
    ? { ...workspace, icon: selection }
    : { ...workspace };

const encodeSvgAsDataUrl = (svgContent: string): string => {
  const svgBytes = new TextEncoder().encode(svgContent);
  let binString = "";
  for (const byte of svgBytes) {
    binString += String.fromCharCode(byte);
  }
  return `data:image/svg+xml;base64,${btoa(binString)}`;
};

export class WorkspaceIcons {
  private readonly resolvedIcons = new Map<string, string>();
  public readonly workspaceIcons = new Set(
    WORKSPACE_ICON_REGISTRY.map((entry) => entry.alias),
  );

  get workspaceIconsArray(): string[] {
    return Array.from(this.workspaceIcons);
  }

  get registry(): readonly WorkspaceIconRegistryEntry[] {
    return WORKSPACE_ICON_REGISTRY;
  }

  constructor() {
    const moduleStrings = import.meta.glob("../icons/*.svg", {
      query: "?raw",
      import: "default",
      eager: true,
    }) as ModuleStrings;

    const normalizeGlobKey = (key: string): string => {
      return key.split("/").pop()?.replace(/\?raw$/, "") ?? key;
    };

    const registeredAssetPaths = new Set<string>(
      WORKSPACE_ICON_REGISTRY.map((entry) => normalizeGlobKey(entry.assetPath)),
    );
    const bundledAssetPaths = new Set(
      Object.keys(moduleStrings).map(normalizeGlobKey),
    );
    if (
      bundledAssetPaths.size !== WORKSPACE_ICON_REGISTRY.length ||
      [...bundledAssetPaths].some((path) => !registeredAssetPaths.has(path))
    ) {
      throw new Error(
        "Bundled workspace SVGs do not match the 34-entry icon registry",
      );
    }

    const moduleStringsByBasename = new Map<string, string>();
    for (const [key, value] of Object.entries(moduleStrings)) {
      const basename = normalizeGlobKey(key);
      moduleStringsByBasename.set(basename, value as string);
    }

    for (const entry of WORKSPACE_ICON_REGISTRY) {
      const basename = normalizeGlobKey(entry.assetPath);
      const svgContent = moduleStringsByBasename.get(basename);
      if (typeof svgContent !== "string") {
        throw new Error(`Missing bundled workspace icon: ${entry.assetPath}`);
      }
      this.resolvedIcons.set(
        entry.canonicalId,
        encodeSvgAsDataUrl(svgContent),
      );
    }
  }

  public getWorkspaceIconCanonicalId(
    icon: unknown,
  ): WorkspaceIconRegistryEntry["canonicalId"] {
    return (getExactRegistryEntry(icon) ?? FALLBACK_ENTRY).canonicalId;
  }

  public isCanonicalWorkspaceIconId(
    icon: unknown,
  ): icon is WorkspaceIconRegistryEntry["canonicalId"] {
    return isCanonicalWorkspaceIconId(icon);
  }

  public getWorkspaceIconUrl(icon: unknown): string {
    const entry = getExactRegistryEntry(icon) ?? FALLBACK_ENTRY;
    const resolved = this.resolvedIcons.get(entry.canonicalId);
    if (resolved) {
      return resolved;
    }

    const fallback = this.resolvedIcons.get(FALLBACK_ENTRY.canonicalId);
    if (!fallback) {
      throw new Error("Bundled fingerprint workspace icon is unavailable");
    }
    return fallback;
  }
}
