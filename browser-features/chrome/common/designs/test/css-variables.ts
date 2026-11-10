// SPDX-License-Identifier: MPL-2.0
//
// Custom-property bookkeeping for css-variables.test.ts: who defines and who
// reads each `--name`, on Floorp's side (sources) and on Gecko's side (the
// running Runtime's own chrome/toolkit files plus the live chrome window).

import {
  blankCssComments,
  lexTs,
  normalizeKey,
  RAW_CSS,
  RAW_TS,
} from "./css-sources.ts";

export type VarUses = {
  /** name -> first place that defines it. */
  defs: Map<string, string>;
  /** name -> first place that reads it with `var()`. */
  reads: Map<string, string>;
  /** Names that appear as a bare `"--name"` string (JS/TS); either role. */
  mentions: Map<string, string>;
};

export function emptyUses(): VarUses {
  return { defs: new Map(), reads: new Map(), mentions: new Map() };
}

function note(map: Map<string, string>, name: string, where: string): void {
  if (!map.has(name)) map.set(name, where);
}

const DEF_RE = /(?<![\w-])(--[A-Za-z_][\w-]*)\s*:(?!:)/g;
const PROPERTY_RE = /@property\s+(--[A-Za-z_][\w-]*)/g;
const READ_RE = /var\(\s*(--[A-Za-z_][\w-]*)/g;
const NAME_RE = /^--[A-Za-z_][\w-]*$/;
const JS_NAME_RE = /(?<![\w-])--[A-Za-z_][\w-]*/g;

/** Line lookup for a text, built on first use. */
function lineFinder(text: string): (index: number) => number {
  let starts: number[] | null = null;
  return (index) => {
    if (!starts) {
      starts = [0];
      for (let i = text.indexOf("\n"); i >= 0; i = text.indexOf("\n", i + 1)) {
        starts.push(i + 1);
      }
    }
    let lo = 0;
    let hi = starts.length - 1;
    while (lo < hi) {
      const mid = (lo + hi + 1) >> 1;
      if (starts[mid] <= index) lo = mid;
      else hi = mid - 1;
    }
    return lo + 1;
  };
}

/** Record the definitions and `var()` reads of a CSS text. */
export function scanCss(
  css: string,
  label: string,
  uses: VarUses,
  lineOffset = 0,
): void {
  const text = blankCssComments(css);
  const lineAt = lineFinder(text);
  const where = (index: number) => `${label}:${lineAt(index) + lineOffset}`;
  for (const re of [DEF_RE, PROPERTY_RE]) {
    for (const m of text.matchAll(re)) {
      if (!uses.defs.has(m[1])) uses.defs.set(m[1], where(m.index));
    }
  }
  for (const m of text.matchAll(READ_RE)) {
    if (!uses.reads.has(m[1])) uses.reads.set(m[1], where(m.index));
  }
}

/**
 * Record a TS/TSX source: its template literals are scanned as CSS, and a
 * string literal that is exactly a custom-property name (`"--x"`, as passed
 * to setProperty/getPropertyValue or listed in a table) counts as a mention,
 * i.e. both a possible definition and a possible read. Plain strings that
 * contain `var(--x)` (inline styles) count as reads.
 */
export function scanTs(src: string, label: string, uses: VarUses): void {
  const lexed = lexTs(src);
  const lineAt = lineFinder(src);
  for (const t of lexed.templates) {
    scanCss(t.text, label, uses, lineAt(t.start) - 1);
  }
  for (const s of lexed.strings) {
    if (NAME_RE.test(s)) note(uses.mentions, s, label);
    else if (s.includes("--")) scanCss(s, label, uses);
  }
}

/**
 * Files whose own findings are not reported, with a reason. They are still
 * scanned, so what they define counts for everybody else. Keep this short;
 * prefer an allowlist entry for a single variable.
 */
export const UNREPORTED_SOURCES: Record<string, string> = {
  "browser-features/chrome/utils/modal/styles.css":
    "generated Chakra UI token dump (--chakra-*), not design CSS",
};

/**
 * Lepton, fluerial and every chrome CSS / TS source of Floorp. Unreported
 * sources are scanned last, so a name they share with another file is
 * located in that other file.
 */
export function scanFloorpSources(): VarUses {
  const uses = emptyUses();
  const css = Object.entries(RAW_CSS).map(([key, text]) =>
    [normalizeKey(key), text] as const
  );
  const last = ([file]: readonly [string, string]) =>
    file in UNREPORTED_SOURCES ? 1 : 0;
  css.sort((a, b) => last(a) - last(b));
  for (const [file, text] of css) scanCss(text, file, uses);
  for (const [key, src] of Object.entries(RAW_TS)) {
    const file = normalizeKey(key);
    if (!src.includes("--")) continue;
    scanTs(src, file, uses);
  }
  return uses;
}

// ---------------------------------------------------------------------------
// Gecko side
// ---------------------------------------------------------------------------

const PLATFORM_FILE_RE = /\.(css|mjs|js|xhtml|html)$/;
/** Floorp's own code inside the Runtime (packaged builds) is not Gecko. */
const PLATFORM_SKIP_RE = /(^|[/\\])noraneko[^/\\]*([/\\]|$)/;
/** Unpacked (developer) Runtime: the directories that hold chrome files. */
const UNPACKED_DIRS = [
  "chrome",
  "modules",
  "moz-src",
  "actors",
  "res",
  "contentaccessible",
];

function scanPlatformFile(path: string, text: string, uses: VarUses): void {
  if (path.endsWith(".css")) {
    scanCss(text, path, uses);
    return;
  }
  for (const m of text.matchAll(JS_NAME_RE)) note(uses.mentions, m[0], path);
  if (text.includes("var(")) {
    for (const m of text.matchAll(READ_RE)) note(uses.reads, m[1], path);
  }
}

function scanOmni(omni: nsIFile, uses: VarUses): number {
  const { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs",
  );
  const zip = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader,
  );
  zip.open(omni);
  let files = 0;
  try {
    const entries = zip.findEntries("*");
    while (entries.hasMore()) {
      const name = entries.getNext();
      if (!PLATFORM_FILE_RE.test(name) || PLATFORM_SKIP_RE.test(name)) {
        continue;
      }
      const stream = zip.getInputStream(name);
      const text = NetUtil.readInputStreamToString(stream, stream.available(), {
        charset: "UTF-8",
      });
      stream.close();
      scanPlatformFile(`${omni.parent?.leafName}/omni.ja!/${name}`, text, uses);
      files++;
    }
  } finally {
    zip.close();
  }
  return files;
}

async function scanDir(
  dir: string,
  label: string,
  uses: VarUses,
): Promise<number> {
  let files = 0;
  let children: string[];
  try {
    children = await IOUtils.getChildren(dir);
  } catch {
    return 0;
  }
  for (const child of children) {
    const leaf = PathUtils.filename(child);
    if (PLATFORM_SKIP_RE.test(leaf)) continue;
    const info = await IOUtils.stat(child);
    if (info.type === "directory") {
      files += await scanDir(child, `${label}/${leaf}`, uses);
    } else if (PLATFORM_FILE_RE.test(leaf)) {
      scanPlatformFile(`${label}/${leaf}`, await IOUtils.readUTF8(child), uses);
      files++;
    }
  }
  return files;
}

export type PlatformScan = { uses: VarUses; files: number; layout: string };

/**
 * Gecko's own definitions and reads, from the Runtime this browser runs:
 * `omni.ja` of GreD and of the browser app dir when packaged, or the
 * unpacked chrome/module directories of a developer Runtime. Floorp's own
 * `noraneko*` content is skipped. JS/HTML files contribute bare `--name`
 * mentions (ThemeVariableMap, setProperty, inline styles), counted as both
 * definition and read.
 */
export async function scanPlatform(): Promise<PlatformScan> {
  const uses = emptyUses();
  let files = 0;
  const layouts: string[] = [];
  for (const key of ["GreD", "XCurProcD"]) {
    const root = Services.dirsvc.get(key, Ci.nsIFile);
    const omni = root.clone();
    omni.append("omni.ja");
    if (omni.exists()) {
      files += scanOmni(omni, uses);
      layouts.push(`${key}:omni.ja`);
      continue;
    }
    for (const sub of UNPACKED_DIRS) {
      files += await scanDir(
        PathUtils.join(root.path, sub),
        `${root.leafName}/${sub}`,
        uses,
      );
    }
    layouts.push(`${key}:unpacked`);
  }
  return { uses, files, layout: layouts.join(", ") };
}

/** Custom properties that are set on the chrome window's root right now. */
export function liveRootProperties(): Set<string> {
  const out = new Set<string>();
  const computed = globalThis.getComputedStyle(document.documentElement);
  if (!computed) return out;
  for (let i = 0; i < computed.length; i++) {
    const name = computed[i];
    if (name.startsWith("--")) out.add(name);
  }
  return out;
}

export type VarFindings = {
  /** Read by Floorp with var(), defined nowhere. name -> first read. */
  undefinedReads: Map<string, string>;
  /** Defined by Floorp, read by nobody (Floorp or Gecko). name -> first def. */
  unreadDefs: Map<string, string>;
};

export function findVarProblems(
  floorp: VarUses,
  platform: VarUses,
  live: Set<string>,
): VarFindings {
  const defined = (name: string): boolean =>
    floorp.defs.has(name) || floorp.mentions.has(name) ||
    platform.defs.has(name) || platform.mentions.has(name) || live.has(name);
  const read = (name: string): boolean =>
    floorp.reads.has(name) || floorp.mentions.has(name) ||
    platform.reads.has(name) || platform.mentions.has(name);
  const reported = (where: string): boolean =>
    !Object.keys(UNREPORTED_SOURCES).some((file) =>
      where.startsWith(`${file}:`)
    );
  const undefinedReads = new Map<string, string>();
  for (const [name, where] of floorp.reads) {
    if (!defined(name) && reported(where)) undefinedReads.set(name, where);
  }
  const unreadDefs = new Map<string, string>();
  for (const [name, where] of floorp.defs) {
    if (!read(name) && reported(where)) unreadDefs.set(name, where);
  }
  return { undefinedReads, unreadDefs };
}
