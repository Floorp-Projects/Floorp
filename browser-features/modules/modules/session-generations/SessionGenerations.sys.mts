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
 */

declare const PathUtils: {
  join(...parts: string[]): string;
  filename(path: string): string;
  readonly profileDir: string;
};

interface IOUtilsLike {
  getChildren(path: string): Promise<string[]>;
  stat(path: string): Promise<{ size: number }>;
  read(path: string, options?: { decompress?: boolean }): Promise<Uint8Array>;
}

declare const IOUtils: IOUtilsLike;

const ROTATION_PREFIX = "rotation-";
const ROTATION_SUFFIX = ".jsonlz4";

interface Generation {
  path: string;
  timestamp: number;
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

function formatTimestamp(ts: number): string {
  const d = new Date(ts);
  const pad = (n: number) => String(n).padStart(2, "0");
  return (
    `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ` +
    `${pad(d.getHours())}:${pad(d.getMinutes())}`
  );
}

async function restoreGeneration(win: Window, gen: Generation): Promise<void> {
  const ok = Services.prompt.confirm(
    win as never,
    "Restore Session Generation",
    "Replace the current session with the generation from " +
      formatTimestamp(gen.timestamp) +
      "? Current tabs and windows will be closed.",
  );
  if (!ok) {
    return;
  }
  try {
    const bytes = await IOUtils.read(gen.path, { decompress: true });
    const json = new TextDecoder().decode(bytes);
    const { SessionStore } = ChromeUtils.importESModule(
      "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
    ) as { SessionStore: { setBrowserState(state: string): void } };
    SessionStore.setBrowserState(json);
  } catch (e) {
    console.error("[SessionGenerations] restore failed", e);
    Services.prompt.alert(
      win as never,
      "Restore Session Generation",
      "Restore failed: " + (e instanceof Error ? e.message : String(e)),
    );
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
  menu.setAttribute("label", "Restore Session from Generation");
  const menupopup = doc.createXULElement("menupopup");
  menupopup.addEventListener("popupshowing", () => {
    while (menupopup.firstChild) {
      menupopup.firstChild.remove();
    }
    listGenerations().then((gens) => {
      if (!gens.length) {
        const empty = doc.createXULElement("menuitem");
        empty.setAttribute("label", "(no generations)");
        empty.setAttribute("disabled", "true");
        menupopup.appendChild(empty);
        return;
      }
      for (const gen of gens) {
        const item = doc.createXULElement("menuitem");
        item.setAttribute("label", formatTimestamp(gen.timestamp));
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
