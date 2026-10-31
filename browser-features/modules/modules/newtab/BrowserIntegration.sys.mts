// SPDX-License-Identifier: MPL-2.0
import type { PinnedSite, WorkspaceInfo } from "#libs/newtab/types.ts";
import { NewTabSettingsError } from "#libs/newtab/settings.ts";
import { SettingsService } from "./SettingsService.sys.mts";

/** Only the calling actor's chrome window is accepted; never select a global window. */
export interface BrowserWindowContext {
  closed?: boolean;
  document?: Document;
  gURLBar?: { focus(): void; select?(): void };
  gBrowser?: { selectedBrowser?: unknown };
  openTrustedLinkIn?: (
    url: string,
    where: string,
    options: {
      triggeringPrincipal: unknown;
      relatedToCurrent: boolean;
      allowInheritPrincipal: boolean;
    },
  ) => void;
  workspacesFuncs?: { getSelectedWorkspaceID(): unknown };
}

const WORKSPACE_ENABLED_PREF = "floorp.workspaces.enabled";
const WORKSPACE_STORE_PREF = "floorp.workspaces.v4.store";
const MAX_IMAGE_BYTES = 25 * 1024 * 1024;
const IMAGE_EXTENSION = /\.(jpe?g|png|avif|webp|gif|bmp)$/i;
const DEFAULT_SUGGESTED_SITES: PinnedSite[] = [
  { url: "https://youtube.com", title: "YouTube" },
  { url: "https://x.com", title: "X" },
  { url: "https://www.reddit.com/", title: "Reddit" },
  { url: "https://wikipedia.org/", title: "Wikipedia" },
  { url: "https://instagram.com", title: "Instagram" },
];

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

export function getWorkspaceSnapshot(
  win: BrowserWindowContext | null,
): { workspace: WorkspaceInfo | null; workspaces: WorkspaceInfo[] } {
  const empty = { workspace: null, workspaces: [] };
  if (!win || win.closed) return empty;
  try {
    if (!Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF, true)) return empty;
    const raw: unknown = JSON.parse(
      Services.prefs.getStringPref(WORKSPACE_STORE_PREF, "{}"),
    );
    if (!isRecord(raw)) return empty;
    const values = new Map<string, WorkspaceInfo>();
    const entries: unknown[] = Array.isArray(raw.data)
      ? raw.data
      : isRecord(raw.data)
      ? Object.entries(raw.data)
      : [];
    for (const entry of entries) {
      if (!Array.isArray(entry) || entry.length !== 2) continue;
      const [id, workspace] = entry as unknown[];
      if (
        typeof id !== "string" ||
        !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(
          id,
        ) || !isRecord(workspace) ||
        typeof workspace.name !== "string"
      ) continue;
      values.set(id, { id, title: workspace.name });
    }
    const order = Array.isArray(raw.order)
      ? raw.order.filter((id): id is string => typeof id === "string")
      : [];
    const ids = [...new Set([...order, ...values.keys()])];
    const workspaces = ids.flatMap((id) =>
      values.has(id) ? [values.get(id)!] : []
    );
    const selected = win.workspacesFuncs?.getSelectedWorkspaceID();
    return {
      workspace: typeof selected === "string"
        ? values.get(selected) ?? null
        : null,
      workspaces,
    };
  } catch {
    // Broken workspace metadata must neither reset it nor select another window.
    return empty;
  }
}

export function focusUrlBar(win: BrowserWindowContext | null): void {
  if (!win || win.closed) {
    throw new NewTabSettingsError(
      "UNAVAILABLE",
      "The browser window is no longer available.",
    );
  }
  const command = win.document?.getElementById("Browser:OpenLocation") as
    | (Element & { doCommand?: () => void })
    | null
    | undefined;
  if (typeof command?.doCommand === "function") {
    command.doCommand();
    return;
  }
  if (win.gURLBar) {
    win.gURLBar.focus();
    win.gURLBar.select?.();
    return;
  }
  throw new NewTabSettingsError(
    "UNAVAILABLE",
    "The address bar is not available.",
  );
}

export function openHub(win: BrowserWindowContext | null): void {
  if (!win || win.closed || typeof win.openTrustedLinkIn !== "function") {
    throw new NewTabSettingsError(
      "UNAVAILABLE",
      "The browser window is no longer available.",
    );
  }
  win.openTrustedLinkIn("about:hub#/features/newtab", "tab", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    relatedToCurrent: true,
    allowInheritPrincipal: false,
  });
}

function normalHistoryUrl(value: unknown): string | null {
  if (typeof value !== "string") return null;
  try {
    const url = new URL(value);
    return url.protocol === "https:" || url.protocol === "http:"
      ? url.href
      : null;
  } catch {
    return null;
  }
}

function historyRows(value: unknown, excluded: Set<string>): PinnedSite[] {
  if (!Array.isArray(value)) return [];
  const seen = new Set<string>();
  return value.flatMap((item): PinnedSite[] => {
    if (!isRecord(item)) return [];
    // Platform advertising is not a history entry or part of Floorp's two slots.
    if (
      item.sponsored_position || item.sponsored_tile_id ||
      item.type === "sponsored"
    ) return [];
    const url = normalHistoryUrl(item.url);
    if (!url || excluded.has(url) || seen.has(url)) return [];
    seen.add(url);
    const title = typeof item.title === "string" && item.title
      ? item.title
      : typeof item.label === "string" && item.label
      ? item.label
      : url;
    return [{ url, title: title.slice(0, 1024) }];
  }).slice(0, 50);
}

export async function getHistorySites(): Promise<PinnedSite[]> {
  const settings = SettingsService.get();
  const excluded = new Set(
    [
      ...settings.topSites.blocked,
      ...settings.topSites.pinned.map((site) => site.url),
    ].map(normalHistoryUrl).filter((url): url is string => url !== null),
  );
  let sites: PinnedSite[] = [];
  try {
    const { AboutNewTab } = ChromeUtils.importESModule(
      "resource:///modules/AboutNewTab.sys.mjs",
    ) as { AboutNewTab: { getTopSites(): unknown } };
    sites = historyRows(await AboutNewTab.getTopSites(), excluded);
  } catch { /* Fall through to Places-backed Activity Stream links. */ }
  if (sites.length === 0) {
    try {
      const { NewTabUtils } = ChromeUtils.importESModule(
        "resource://gre/modules/NewTabUtils.sys.mjs",
      ) as {
        NewTabUtils: {
          activityStreamLinks: { getTopSites(): Promise<unknown> };
        };
      };
      sites = historyRows(
        await NewTabUtils.activityStreamLinks.getTopSites(),
        excluded,
      );
    } catch {
      /* Preserve the existing suggested-site fallback when history is unavailable. */
    }
  }
  return sites.length <= 3
    ? historyRows(DEFAULT_SUGGESTED_SITES, excluded)
    : sites;
}

export async function pickBackgroundFolder(
  browsingContext: BrowsingContext,
): Promise<{ path: string | null }> {
  const picker = Cc["@mozilla.org/filepicker;1"].createInstance(
    Ci.nsIFilePicker,
  );
  picker.init(browsingContext, "Floorp", Ci.nsIFilePicker.modeGetFolder);
  if (typeof Ci.nsIFilePicker.filterAll === "number") {
    picker.appendFilters(Ci.nsIFilePicker.filterAll);
  }
  const result = await new Promise<number>((resolve) => picker.open(resolve));
  return {
    path: result === Ci.nsIFilePicker.returnOK
      ? picker.file?.path ?? null
      : null,
  };
}

function imageMime(bytes: Uint8Array, filename: string): string | null {
  const extension = filename.toLowerCase().split(".").at(-1);
  const begins = (...prefix: number[]) =>
    prefix.every((byte, index) => bytes[index] === byte);
  const ascii = (start: number, end: number) =>
    String.fromCharCode(...bytes.slice(start, end));
  if (
    (extension === "jpg" || extension === "jpeg") && begins(0xff, 0xd8, 0xff)
  ) return "image/jpeg";
  if (
    extension === "png" &&
    begins(0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)
  ) return "image/png";
  if (extension === "gif" && ["GIF87a", "GIF89a"].includes(ascii(0, 6))) {
    return "image/gif";
  }
  if (
    extension === "webp" && ascii(0, 4) === "RIFF" && ascii(8, 12) === "WEBP"
  ) return "image/webp";
  if (extension === "bmp" && ascii(0, 2) === "BM") return "image/bmp";
  if (
    extension === "avif" && ascii(4, 8) === "ftyp" &&
    /avif|avis/.test(ascii(8, 64))
  ) return "image/avif";
  return null;
}

/** No path supplied by a page is accepted; only the saved native selection is read. */
export async function getFolderImage(): Promise<
  { image: string | null; fileName: string | null }
> {
  const empty = { image: null, fileName: null };
  const directory = SettingsService.get().background.folderPath;
  if (!directory || !PathUtils.isAbsolute(directory)) return empty;
  try {
    if ((await IOUtils.stat(directory)).type !== "directory") return empty;
    const candidates = (await IOUtils.getChildren(directory)).filter((path) =>
      IMAGE_EXTENSION.test(PathUtils.filename(path))
    );
    const start = Math.floor(Math.random() * candidates.length);
    // Skip corrupt/oversized candidates without scanning an unbounded directory.
    for (
      let attempt = 0;
      attempt < Math.min(candidates.length, 20);
      attempt++
    ) {
      const path = candidates[(start + attempt) % candidates.length];
      try {
        const info = await IOUtils.stat(path);
        if (
          info.type !== "regular" || !info.size || info.size > MAX_IMAGE_BYTES
        ) continue;
        const bytes = await IOUtils.read(path, {
          maxBytes: MAX_IMAGE_BYTES + 1,
        });
        if (bytes.length === 0 || bytes.length > MAX_IMAGE_BYTES) continue;
        const filename = PathUtils.filename(path);
        const mime = imageMime(bytes, filename);
        if (!mime) continue;
        if (
          SettingsService.get().background.folderPath !== directory
        ) return empty;
        const base64 = ChromeUtils.base64URLEncode(Uint8Array.from(bytes), {
          pad: true,
        })
          .replace(/-/g, "+").replace(/_/g, "/");
        return { image: `data:${mime};base64,${base64}`, fileName: filename };
      } catch {
        /* A disappeared or unreadable file must not expose its path in logs. */
      }
    }
  } catch { /* Missing directories fall back to the normal page surface. */ }
  return empty;
}
