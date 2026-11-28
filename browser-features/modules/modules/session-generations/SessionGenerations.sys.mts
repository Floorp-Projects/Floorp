/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SessionGenerations — restore UI for time-depth rotation copies.
 *
 * The runtime patch `sessionstore-time-depth-rotation.patch` keeps timestamped
 * generations of the recovery session file in sessionstore-backups. This
 * module lists them in the Tools menu and restores a chosen generation in
 * place via SessionStore.setBrowserState, without a browser restart.
 *
 * The wording follows plain-language principles: reassure first in error
 * paths, say what happens before it happens, name concrete times and counts
 * instead of technical terms.
 */

declare const PathUtils: {
  join(...parts: string[]): string;
  filename(path: string): string;
  readonly profileDir: string;
};

interface IOUtilsLike {
  getChildren(path: string): Promise<string[]>;
  read(
    path: string,
    options?: { decompress?: boolean; decrypt?: string },
  ): Promise<Uint8Array>;
}

declare const IOUtils: IOUtilsLike;

const ROTATION_PREFIX = "rotation-";
const ROTATION_SUFFIX = ".jsonlz4";

const isJa = (Services.locale.appLocalesAsBCP47?.[0] || "en").startsWith("ja");

const strings = {
  menuLabel: isJa
    ? "タブを前の状態に戻す"
    : "Bring back tabs from an earlier time",
  empty: isJa
    ? "まだコピーがありません。使い続けると、1時間に1つほどここに現れます"
    : "No saved copies yet. Copies appear here about once an hour as you browse",
  justNow: isJa ? "たった今" : "just now",
  minutesAgo: (n: number) => (isJa ? `${n}分前` : `${n} minutes ago`),
  hoursAgo: (n: number) => (isJa ? `${n}時間前` : `${n} hours ago`),
  yesterday: isJa ? "昨日" : "yesterday",
  daysAgo: (n: number) => (isJa ? `${n}日前` : `${n} days ago`),
  tabsAndWindows: (tabs: number, wins: number) =>
    isJa
      ? `タブ${tabs}件・ウィンドウ${wins}つ`
      : `${tabs} tab${tabs === 1 ? "" : "s"} in ${wins} window${wins === 1 ? "" : "s"}`,
  confirmTitle: isJa ? "タブを元に戻しますか?" : "Bring back these tabs?",
  confirmBody: (time: string, tabs: number) =>
    isJa
      ? `いま開いているタブは閉じて、${time}のタブ${tabs}件を開き直します。` +
        "数秒かかります。ブックマークと履歴はそのまま残ります。"
      : `The tabs you have open now will close, and the ${tabs} tab${tabs === 1 ? "" : "s"} ` +
        `from ${time} will open instead. This takes a few seconds. ` +
        "Your bookmarks and history stay as they are.",
  failTitle: isJa ? "何も変わっていません" : "Nothing was changed",
  failBody: isJa
    ? "タブはそのまま残っています。保存されたコピーが読めませんでした。" +
      "一覧の、もう少し古いコピーを試すことができます。"
    : "Your tabs are still here, exactly as they were. This saved copy " +
      "could not be read — it may be damaged. You can try an older copy " +
      "from the list.",
};

interface Generation {
  path: string;
  timestamp: number;
}

interface GenerationCounts {
  tabs: number;
  windows: number;
}

function backupsDir(): string {
  return PathUtils.join(PathUtils.profileDir, "sessionstore-backups");
}

async function listGenerations(): Promise<Generation[]> {
  let children: string[];
  try {
    children = await IOUtils.getChildren(backupsDir());
  } catch {
    return [];
  }
  const out: Generation[] = [];
  for (const path of children) {
    const name = PathUtils.filename(path);
    if (!name.startsWith(ROTATION_PREFIX) || !name.endsWith(ROTATION_SUFFIX)) {
      continue;
    }
    const ts = parseInt(
      name.slice(ROTATION_PREFIX.length, -ROTATION_SUFFIX.length),
      10,
    );
    if (!isNaN(ts)) {
      out.push({ path, timestamp: ts });
    }
  }
  out.sort((a, b) => b.timestamp - a.timestamp);
  return out;
}

function relativeTime(ts: number): string {
  const mins = Math.floor((Date.now() - ts) / 60000);
  if (mins < 1) {
    return strings.justNow;
  }
  if (mins < 60) {
    return strings.minutesAgo(mins);
  }
  const hours = Math.floor(mins / 60);
  if (hours < 24) {
    return strings.hoursAgo(hours);
  }
  const days = Math.floor(hours / 24);
  if (days <= 1) {
    return strings.yesterday;
  }
  return strings.daysAgo(days);
}

// Rotation copies are immutable once written, so counts never go stale.
const countsCache = new Map<string, GenerationCounts | null>();

function encryptionAvailable(): boolean {
  return Services.prefs.getBoolPref(
    "browser.sessionstore.encryption.available",
    false,
  );
}

async function readGenerationJson(path: string): Promise<string> {
  // Rotation copies inherit the recovery file's encryption when the
  // experimental sessionstore encryption was on at write time. Passing
  // decrypt whenever encryption is available is safe: unencrypted files
  // pass through untouched, matching SessionFile's own read path.
  const bytes = await IOUtils.read(path, {
    decompress: true,
    ...(encryptionAvailable() ? { decrypt: "sessionstore" } : {}),
  });
  return new TextDecoder().decode(bytes);
}

async function generationCounts(path: string): Promise<GenerationCounts | null> {
  if (countsCache.has(path)) {
    return countsCache.get(path) ?? null;
  }
  let counts: GenerationCounts | null = null;
  try {
    const json = await readGenerationJson(path);
    const parsed = JSON.parse(json) as {
      windows?: { tabs?: unknown[] }[];
    };
    const windows = parsed.windows?.length ?? 0;
    const tabs = (parsed.windows ?? []).reduce(
      (sum, win) => sum + (win.tabs?.length ?? 0),
      0,
    );
    counts = { tabs, windows };
  } catch {
    counts = null;
  }
  countsCache.set(path, counts);
  return counts;
}

async function restoreGeneration(win: Window, gen: Generation): Promise<void> {
  const counts = await generationCounts(gen.path);
  const time = relativeTime(gen.timestamp);
  const ok = Services.prompt.confirm(
    win as never,
    strings.confirmTitle,
    strings.confirmBody(time, counts?.tabs ?? 0),
  );
  if (!ok) {
    return;
  }
  try {
    const json = await readGenerationJson(gen.path);
    const { SessionStore } = ChromeUtils.importESModule(
      "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
    ) as { SessionStore: { setBrowserState(state: string): void } };
    SessionStore.setBrowserState(json);
  } catch (e) {
    console.error("[SessionGenerations] restore failed", e);
    Services.prompt.alert(win as never, strings.failTitle, strings.failBody);
  }
}

function injectMenu(win: Window): void {
  const doc = (win as unknown as { document: Document }).document;
  if (doc.getElementById("sessiongenerations-menu")) {
    return;
  }
  const popup = doc.getElementById("menu_ToolsPopup");
  if (!popup) {
    return;
  }
  const menu = doc.createXULElement("menu");
  menu.id = "sessiongenerations-menu";
  menu.setAttribute("label", strings.menuLabel);
  const menupopup = doc.createXULElement("menupopup");
  menupopup.addEventListener("popupshowing", () => {
    while (menupopup.firstChild) {
      menupopup.firstChild.remove();
    }
    listGenerations().then(async (gens) => {
      if (!gens.length) {
        const empty = doc.createXULElement("menuitem");
        empty.setAttribute("label", strings.empty);
        empty.setAttribute("disabled", "true");
        menupopup.appendChild(empty);
        return;
      }
      for (const gen of gens) {
        const item = doc.createXULElement("menuitem");
        const counts = await generationCounts(gen.path);
        const time = relativeTime(gen.timestamp);
        // include the clock time as well: two generations from the same
        // hour would otherwise be indistinguishable when their tab counts
        // happen to match
        const d = new Date(gen.timestamp);
        const clock =
          String(d.getHours()).padStart(2, "0") +
          ":" +
          String(d.getMinutes()).padStart(2, "0");
        item.setAttribute(
          "label",
          counts
            ? `${time} (${clock}) — ${strings.tabsAndWindows(counts.tabs, counts.windows)}`
            : `${time} (${clock})`,
        );
        item.addEventListener("command", () => {
          restoreGeneration(win, gen).catch((e) =>
            console.error("[SessionGenerations] restore", e),
          );
        });
        menupopup.appendChild(item);
      }
    }).catch((e) => console.error("[SessionGenerations] list", e));
  });
  menu.appendChild(menupopup);
  popup.appendChild(menu);
}

function injectIntoAllWindows(): void {
  for (const win of Services.wm.getEnumerator("navigator:browser")) {
    try {
      injectMenu(win as unknown as Window);
    } catch (e) {
      console.error("[SessionGenerations] menu injection", e);
    }
  }
}

Services.obs.addObserver(() => {
  injectIntoAllWindows();
  Services.obs.addObserver((win) => {
    (win as Window).addEventListener(
      "load",
      () => {
        try {
          injectMenu(win as Window);
        } catch (e) {
          console.error("[SessionGenerations] menu injection", e);
        }
      },
      { once: true },
    );
  }, "domwindowopened");
}, "sessionstore-windows-restored");
