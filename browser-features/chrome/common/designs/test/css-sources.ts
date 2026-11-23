// SPDX-License-Identifier: MPL-2.0
//
// Shared source discovery for the chrome CSS health tests
// (lwtheme-selectors, css-dropped-rules, css-variables).
//
// Every chrome CSS file, the vendored skins (Lepton, fluerial) and the CSS
// that lives in TypeScript template strings are collected here, so a new
// file or a re-synced leptonChrome.css is covered without editing a list.
//
// Three kinds of source come out of `collectCssSources()`:
// - `css`: a .css file, verbatim.
// - `css-ts`: a string export of a `*.css.ts` module, i.e. the exact CSS the
//   feature injects (generated tables included).
// - `ts-template`: a template literal in any other TS/TSX file that parses as
//   a complete stylesheet. `${…}` is replaced by the value of a same-file
//   string constant when there is one, otherwise by an identifier
//   placeholder, so the selector/at-rule structure survives.

export const RAW_CSS: Record<string, string> = import.meta.glob<string>(
  [
    "../../../**/*.css",
    "@nora/skin/**/*.css",
    "!**/test/**",
    "!**/node_modules/**",
  ],
  { query: "?raw", import: "default", eager: true },
);

export const RAW_TS: Record<string, string> = import.meta.glob<string>(
  [
    "../../../**/*.{ts,tsx,mts}",
    "#modules/**/*.{ts,mts}",
    "!**/test/**",
    "!**/*.test.ts",
    "!**/*.d.ts",
    "!**/node_modules/**",
  ],
  { query: "?raw", import: "default", eager: true },
);

const CSS_TS_MODULES: Record<string, Record<string, unknown>> = import.meta
  .glob<Record<string, unknown>>(
    ["../../../**/*.css.ts", "!**/test/**", "!**/node_modules/**"],
    { eager: true },
  );

export type CssSourceKind = "css" | "css-ts" | "ts-template";

export type CssSource = {
  kind: CssSourceKind;
  /** Repository-relative path of the file the CSS comes from. */
  file: string;
  /**
   * `file`, plus `#EXPORT` for a `*.css.ts` export (whose line numbers are
   * file lines when the export is a plain template literal, and lines of
   * the exported string otherwise).
   */
  label: string;
  text: string;
  /** Added to a line number inside `text` to get the file line. */
  lineOffset: number;
};

/**
 * Glob keys come back either relative to this file (`../utils/x.ts`) or
 * root-absolute (`/link-features-chrome/common/…`, `@nora/skin` files).
 * Map them to a path under browser-features/ so they read like repository
 * paths in failure messages and can be matched by suffix.
 */
export function normalizeKey(key: string): string {
  const path = resolveKey(key);
  const at = path.lastIndexOf("browser-features/");
  return at > 0 ? path.slice(at) : path;
}

function resolveKey(key: string): string {
  if (key.startsWith(".")) {
    const parts = ["browser-features", "chrome", "common", "designs", "test"];
    for (const segment of key.split("/")) {
      if (segment === "..") parts.pop();
      else if (segment !== ".") parts.push(segment);
    }
    return parts.join("/");
  }
  const chrome = key.indexOf("/link-features-chrome/");
  if (chrome >= 0) {
    return `browser-features/chrome/${
      key.slice(chrome + "/link-features-chrome/".length)
    }`;
  }
  const skin = key.indexOf("/skin/");
  if (skin >= 0) return `browser-features${key.slice(skin)}`;
  return key.replace(/^\/+/, "");
}

export function findKey(record: Record<string, unknown>, suffix: string) {
  const key = Object.keys(record).find((k) =>
    normalizeKey(k).endsWith(`/${suffix}`)
  );
  if (!key) throw new Error(`glob must include ${suffix}`);
  return key;
}

/** Replace a match with spaces, keeping its newlines (line numbers stay). */
function blank(text: string): string {
  return text.replace(/[^\n]/g, " ");
}

/** CSS block comments blanked out; offsets and line numbers are unchanged. */
export function blankCssComments(text: string): string {
  return text.replace(/\/\*[\s\S]*?\*\//g, blank);
}

export function lineAt(text: string, index: number): number {
  let line = 1;
  for (let i = text.indexOf("\n"); i >= 0 && i < index;) {
    line++;
    i = text.indexOf("\n", i + 1);
  }
  return line;
}

// ---------------------------------------------------------------------------
// A small TypeScript lexer: comments, string literals and template literals.
// ---------------------------------------------------------------------------

export type TsTemplate = {
  /** Offset of the opening backtick in the file. */
  start: number;
  /** Raw template text with every `${…}` replaced by `PLACEHOLDER`. */
  text: string;
  /** Source text of each `${…}` expression, in order. */
  exprs: string[];
};

export type TsLexed = {
  templates: TsTemplate[];
  /** Plain `'…'` / `"…"` literal values (escapes are not decoded). */
  strings: string[];
  /** Same-file `const NAME = "…"` / `const NAME = \`…\`` values. */
  constants: Map<string, string>;
};

export const PLACEHOLDER = "\u0000";

const REGEX_PREFIX = /(?:^|[(,=:[!&|?{};+\-*%<>~^]|\breturn|\btypeof)\s*$/;
const CONST_PREFIX =
  /\bconst\s+([A-Za-z_$][\w$]*)\s*(?::\s*[^=;]+)?=\s*(?:\(\s*)?$/;

/**
 * Lex a TS/TSX source just enough to find its string and template literals.
 * Regex literals are skipped with the usual "previous token" heuristic;
 * JSX text is treated as code, which is harmless for this purpose.
 */
export function lexTs(src: string): TsLexed {
  const templates: TsTemplate[] = [];
  const strings: string[] = [];
  const constants = new Map<string, string>();
  let code = "";

  const readString = (i: number, quote: string): number => {
    let j = i + 1;
    while (j < src.length && src[j] !== quote && src[j] !== "\n") {
      if (src[j] === "\\") j++;
      j++;
    }
    return j;
  };

  // Returns the index of the closing backtick.
  const readTemplate = (start: number): number => {
    let text = "";
    const exprs: string[] = [];
    let j = start + 1;
    while (j < src.length && src[j] !== "`") {
      if (src[j] === "\\") {
        text += src.slice(j, j + 2);
        j += 2;
      } else if (src[j] === "$" && src[j + 1] === "{") {
        const exprStart = j + 2;
        j = readCode(exprStart, true);
        exprs.push(src.slice(exprStart, j));
        text += PLACEHOLDER;
        j++;
      } else {
        text += src[j];
        j++;
      }
    }
    templates.push({ start, text, exprs });
    const name = CONST_PREFIX.exec(code)?.[1];
    if (name && exprs.length === 0) constants.set(name, text);
    return j;
  };

  // Lex code from `i`; with `inExpr`, stop at the `}` closing a `${`.
  const readCode = (i: number, inExpr: boolean): number => {
    let depth = 0;
    let j = i;
    while (j < src.length) {
      const c = src[j];
      const next = src[j + 1];
      if (c === "/" && next === "/") {
        const end = src.indexOf("\n", j);
        j = end < 0 ? src.length : end;
        continue;
      }
      if (c === "/" && next === "*") {
        const end = src.indexOf("*/", j + 2);
        j = end < 0 ? src.length : end + 2;
        code += " ";
        continue;
      }
      if (c === "'" || c === '"') {
        const end = readString(j, c);
        const value = src.slice(j + 1, end);
        strings.push(value);
        const name = CONST_PREFIX.exec(code)?.[1];
        if (name) constants.set(name, value);
        code += "0";
        j = end + 1;
        continue;
      }
      if (c === "`") {
        j = readTemplate(j) + 1;
        code += "0";
        continue;
      }
      if (c === "/" && REGEX_PREFIX.test(code.slice(-40))) {
        let k = j + 1;
        let inClass = false;
        while (k < src.length && src[k] !== "\n") {
          if (src[k] === "\\") k++;
          else if (src[k] === "[") inClass = true;
          else if (src[k] === "]") inClass = false;
          else if (src[k] === "/" && !inClass) break;
          k++;
        }
        code += "0";
        j = k + 1;
        continue;
      }
      if (inExpr) {
        if (c === "{") depth++;
        else if (c === "}") {
          if (depth === 0) return j;
          depth--;
        }
      }
      code += c;
      if (code.length > 4096) code = code.slice(-200);
      j++;
    }
    return j;
  };

  readCode(0, false);
  return { templates, strings, constants };
}

/**
 * Substitute `${…}` in a template: a bare identifier that names a same-file
 * string constant becomes its value (newlines folded), anything else becomes
 * `nora-tpl`. The expression's own newlines are kept so line numbers inside
 * the template stay aligned with the file.
 */
export function resolveTemplate(
  template: TsTemplate,
  constants: Map<string, string>,
): string {
  const chunks = template.text.split(PLACEHOLDER);
  let out = chunks[0];
  for (let n = 1; n < chunks.length; n++) {
    const expr = template.exprs[n - 1] ?? "";
    const newlines = "\n".repeat((expr.match(/\n/g) ?? []).length);
    const id = expr.trim();
    const value = /^[A-Za-z_$][\w$]*$/.test(id) ? constants.get(id) : undefined;
    // `${chunk}` alone on its line splices in a whole stylesheet chunk
    // (e.g. an imported CSS file, which is checked on its own).
    const ownLine = /(^|\n)[ \t]*$/.test(out) &&
      /^[ \t\r]*(\n|$)/.test(chunks[n]);
    if (value !== undefined && !value.includes(PLACEHOLDER)) {
      out += value.replace(/\s+/g, " ") + newlines;
    } else if (ownLine) {
      out += newlines;
    } else {
      out += `nora-tpl${newlines}`;
    }
    out += chunks[n];
  }
  return out;
}

// ---------------------------------------------------------------------------
// Source collection
// ---------------------------------------------------------------------------

/** A template is CSS when it is a whole, balanced stylesheet. */
export type CssShapeCheck = (text: string) => boolean;

export type SkippedTemplate = { file: string; line: number; reason: string };

export type CssSourceSet = {
  sources: CssSource[];
  /**
   * TS template literals that look like CSS (a `{` and a `prop: value;`
   * declaration outside comments, no JS syntax) but are neither a complete stylesheet nor a run of
   * consecutive fragments that add up to one. Reported, never silently
   * dropped.
   */
  skipped: SkippedTemplate[];
};

let cached: CssSourceSet | null = null;

const DECLARATION_RE = /[\w-]+\s*:\s*[^;{}\n]+;/;
/** JS source embedded in a template (polyfills, scripts), not CSS. */
const JS_RE = /\bfunction\b|=>|\b(?:const|let|return|var)\s/;

/** A `{` and a `prop: value;` outside comments, and no JS syntax. */
function looksLikeCss(text: string): boolean {
  const css = blankCssComments(text);
  return css.includes("{") && DECLARATION_RE.test(css) && !JS_RE.test(css);
}

function braceDepth(text: string): number {
  let depth = 0;
  for (const c of text) {
    if (c === "{") depth++;
    else if (c === "}") depth--;
  }
  return depth;
}

/**
 * All CSS sources. `isStylesheet` decides whether a TS template literal is a
 * complete stylesheet and `describe` explains why one is not (see
 * css-rules.ts); they are injected to keep this module free of parsing
 * logic.
 *
 * - `.css` files and `*.css.ts` string exports that contain a `{` are
 *   always returned, so a structural error in them fails the dropped-rule
 *   test instead of hiding the rest of the file.
 * - Other TS template literals are returned when they are a complete
 *   stylesheet, or when up to four consecutive literals of the same file
 *   add up to one (CSS built with `css += \`…\``); the joined text keeps
 *   file line numbers. CSS-looking literals that are neither are listed in
 *   `skipped`.
 */
export function collectCssSources(
  isStylesheet: CssShapeCheck,
  describe: (text: string) => string,
): CssSourceSet {
  if (cached) return cached;
  const sources: CssSource[] = [];
  const skipped: SkippedTemplate[] = [];
  for (const [key, text] of Object.entries(RAW_CSS)) {
    const file = normalizeKey(key);
    sources.push({ kind: "css", file, label: file, text, lineOffset: 0 });
  }
  const cssTsFiles = new Set<string>();
  const rawTs = new Map(
    Object.entries(RAW_TS).map(([key, src]) => [normalizeKey(key), src]),
  );
  for (const [key, mod] of Object.entries(CSS_TS_MODULES)) {
    const file = normalizeKey(key);
    cssTsFiles.add(file);
    const src = rawTs.get(file) ?? "";
    const literals = lexTs(src).templates.filter((t) => t.exprs.length === 0);
    for (const [name, value] of Object.entries(mod)) {
      if (typeof value === "string" && value.includes("{")) {
        // A plain template literal export maps back to file lines; for a
        // generated one (tables, interpolation, concatenation) the line is
        // counted inside the exported string.
        // (Template literals normalize CRLF to LF; checkouts may have CRLF.)
        const literal = literals.find((t) =>
          t.text.replace(/\r\n?/g, "\n") === value
        );
        sources.push({
          kind: "css-ts",
          file,
          label: `${file}#${name}`,
          text: value,
          lineOffset: literal ? lineAt(src, literal.start) - 1 : 0,
        });
      }
    }
  }
  for (const [key, src] of Object.entries(RAW_TS)) {
    const file = normalizeKey(key);
    if (cssTsFiles.has(file) || !src.includes("`")) continue;
    const lexed = lexTs(src);
    const templates = [...lexed.templates].sort((a, b) => a.start - b.start);
    const texts = templates.map((t) => resolveTemplate(t, lexed.constants));
    const lines = templates.map((t) => lineAt(src, t.start));
    const consumed = new Set<number>();
    for (let i = 0; i < templates.length; i++) {
      if (consumed.has(i)) continue;
      const text = texts[i];
      if (!text.includes("{")) continue;
      const push = (css: string) =>
        sources.push({
          kind: "ts-template",
          file,
          label: file,
          text: css,
          lineOffset: lines[i] - 1,
        });
      if (isStylesheet(text)) {
        push(text);
        continue;
      }
      if (!looksLikeCss(text)) continue;
      let joined = text;
      let joinedOk = false;
      if (braceDepth(text) > 0) {
        for (let k = i + 1; k < templates.length && k <= i + 4; k++) {
          const endLine = lines[i] + (joined.match(/\n/g) ?? []).length;
          joined += "\n".repeat(Math.max(0, lines[k] - endLine)) + " " +
            texts[k];
          if (isStylesheet(joined)) {
            for (let c = i + 1; c <= k; c++) consumed.add(c);
            push(joined);
            joinedOk = true;
            break;
          }
          if (braceDepth(joined) <= 0) break;
        }
      }
      if (!joinedOk) {
        skipped.push({ file, line: lines[i], reason: describe(text) });
      }
    }
  }
  cached = { sources, skipped };
  return cached;
}
