// SPDX-License-Identifier: MPL-2.0
//
// Source-level CSS rule walker plus a CSSOM probe that finds rules the
// running Gecko parser drops.
//
// The walker does not try to be a CSS parser. It finds `{…}` blocks and their
// preludes (quote, paren and comment aware), which is all that is needed to
// tag every rule with a probe declaration and to report file:line.

import { blankCssComments } from "./css-sources.ts";

/**
 * At-rules whose block holds rules (or, nested in a style rule,
 * declarations). Every other block at-rule (`@keyframes`, `@font-face`,
 * `@property`, …) is a leaf whose body is not walked.
 */
const GROUPING_AT_RULES = new Set([
  "media",
  "supports",
  "-moz-document",
  "document",
  "layer",
  "container",
  "scope",
  "starting-style",
]);

export type CssBlockKind = "style" | "group" | "leaf";

export type CssBlock = {
  id: number;
  kind: CssBlockKind;
  /** Lower-case at-rule name without `@`; "" for a style rule. */
  atName: string;
  /** Prelude with whitespace collapsed (the selector list for a style rule). */
  prelude: string;
  /** 1-based line of the prelude start within the walked text. */
  line: number;
  /** Offsets of the `{` that opens and the `}` that closes the block. */
  open: number;
  close: number;
  parent: CssBlock | null;
};

export type CssStatement = { atName: string; text: string; line: number };

export type CssWalk = {
  /** Comment-blanked text the offsets refer to. */
  text: string;
  blocks: CssBlock[];
  statements: CssStatement[];
  /** False when braces do not balance. */
  balanced: boolean;
  /** Top-level text that is neither a rule nor an at-statement. */
  stray: string;
};

function lineStarts(text: string): number[] {
  const starts = [0];
  for (let i = text.indexOf("\n"); i >= 0; i = text.indexOf("\n", i + 1)) {
    starts.push(i + 1);
  }
  return starts;
}

function lineOf(starts: number[], index: number): number {
  let lo = 0;
  let hi = starts.length - 1;
  while (lo < hi) {
    const mid = (lo + hi + 1) >> 1;
    if (starts[mid] <= index) lo = mid;
    else hi = mid - 1;
  }
  return lo + 1;
}

/** Index of the `}` matching the `{` at `open` (quote and paren aware). */
function findBlockEnd(css: string, open: number): number {
  let depth = 0;
  let parens = 0;
  let quote = "";
  for (let i = open; i < css.length; i++) {
    const c = css[i];
    if (quote) {
      if (c === "\\") i++;
      else if (c === quote || c === "\n") quote = "";
    } else if (c === '"' || c === "'") {
      quote = c;
    } else if (c === "(") {
      parens++;
    } else if (c === ")") {
      parens = Math.max(0, parens - 1);
    } else if (parens === 0 && c === "{") {
      depth++;
    } else if (parens === 0 && c === "}" && --depth === 0) {
      return i;
    }
  }
  return -1;
}

/**
 * Walk every block of a stylesheet: style rules (nested ones included),
 * grouping at-rules (@media, @supports, @-moz-document, @layer, …) and leaf
 * at-rules, plus the block-less at-statements (@namespace, @import, …).
 */
export function walkCss(source: string): CssWalk {
  const text = blankCssComments(source);
  const starts = lineStarts(text);
  const blocks: CssBlock[] = [];
  const statements: CssStatement[] = [];
  let balanced = true;
  let stray = "";

  const walk = (from: number, to: number, parent: CssBlock | null): void => {
    let segStart = from;
    let parens = 0;
    let quote = "";
    for (let i = from; i < to; i++) {
      const c = text[i];
      if (quote) {
        if (c === "\\") i++;
        else if (c === quote || c === "\n") quote = "";
        continue;
      }
      if (c === '"' || c === "'") quote = c;
      else if (c === "(" || c === "[") parens++;
      else if (c === ")" || c === "]") parens = Math.max(0, parens - 1);
      else if (parens === 0 && c === ";") {
        const seg = text.slice(segStart, i).trim();
        if (seg.startsWith("@")) {
          statements.push({
            atName: (/^@([-\w]+)/.exec(seg)?.[1] ?? "").toLowerCase(),
            text: seg,
            line: lineOf(starts, segStart + text.slice(segStart).search(/\S/)),
          });
        } else if (parent === null && seg) {
          stray += `${seg};`;
        }
        segStart = i + 1;
      } else if (parens === 0 && c === "}") {
        // A `}` with no open block in this range.
        balanced = false;
        segStart = i + 1;
      } else if (parens === 0 && c === "{") {
        const raw = text.slice(segStart, i);
        const lead = raw.search(/\S/);
        const prelude = raw.replace(/\s+/g, " ").trim();
        const close = findBlockEnd(text, i);
        if (close < 0 || close >= to) {
          balanced = false;
          return;
        }
        const atName = prelude.startsWith("@")
          ? (/^@([-\w]+)/.exec(prelude)?.[1] ?? "").toLowerCase()
          : "";
        const kind: CssBlockKind = atName === ""
          ? "style"
          : GROUPING_AT_RULES.has(atName)
          ? "group"
          : "leaf";
        const block: CssBlock = {
          id: blocks.length,
          kind,
          atName,
          prelude,
          line: lineOf(starts, lead < 0 ? i : segStart + lead),
          open: i,
          close,
          parent,
        };
        blocks.push(block);
        if (kind !== "leaf") walk(i + 1, close, block);
        i = close;
        segStart = close + 1;
      }
    }
    if (parent === null) stray += text.slice(segStart, to).trim();
  };

  walk(0, text.length, null);
  return { text, blocks, statements, balanced, stray: stray.trim() };
}

/**
 * True when `text` is a complete stylesheet: balanced braces, at least one
 * style rule, nothing at the top level besides rules and at-statements, and
 * top-level style rules that hold declarations (or nothing).
 * Used to pick the CSS template literals out of TS sources.
 */
export function isStylesheet(text: string): boolean {
  if (!text.includes("{")) return false;
  const walked = walkCss(text);
  if (!walked.balanced || walked.stray !== "") return false;
  const top = walked.blocks.filter((b) => b.parent === null);
  return top.some((b) => b.kind === "style" && /\S/.test(b.prelude)) &&
    top.every((b) => {
      const body = walked.text.slice(b.open + 1, b.close);
      return b.kind !== "style" || !/\S/.test(body) || /[:{]/.test(body);
    });
}

export function hasStyleAncestor(block: CssBlock): boolean {
  for (let p = block.parent; p; p = p.parent) {
    if (p.kind === "style") return true;
  }
  return false;
}

function ancestors(block: CssBlock): CssBlock[] {
  const out: CssBlock[] = [];
  for (let p = block.parent; p; p = p.parent) out.unshift(p);
  return out;
}

/** Split a selector list at its top-level commas. */
export function splitSelectorList(selector: string): string[] {
  const out: string[] = [];
  let depth = 0;
  let quote = "";
  let start = 0;
  for (let i = 0; i < selector.length; i++) {
    const c = selector[i];
    if (quote) {
      if (c === "\\") i++;
      else if (c === quote) quote = "";
    } else if (c === '"' || c === "'") quote = c;
    else if (c === "(" || c === "[") depth++;
    else if (c === ")" || c === "]") depth--;
    else if (c === "," && depth === 0) {
      out.push(selector.slice(start, i).trim());
      start = i + 1;
    }
  }
  out.push(selector.slice(start).trim());
  return out.filter(Boolean);
}

// ---------------------------------------------------------------------------
// CSSOM probing
// ---------------------------------------------------------------------------

const PROBE = "--nora-probe-";

type RuleWithChildren = CSSRule & { cssRules?: CSSRuleList };
type RuleWithStyle = CSSRule & { style?: CSSStyleDeclaration };

/**
 * Where a sheet is parsed.
 * - `chrome`: a constructed CSSStyleSheet of the chrome window. Its URL is
 *   chrome://, so chrome-only syntax is accepted; it is never adopted.
 * - `agent`: a data: URL loaded as an AGENT (user-agent level) sheet into a
 *   windowless chrome document, like the sheets Floorp registers through
 *   nsIStyleSheetService. UA-only syntax such as
 *   `@-moz-document unobservable-document()` is accepted there.
 */
export type ParseMode = "chrome" | "agent";

/** nsIDOMWindowUtils.AGENT_SHEET */
const AGENT_SHEET = 0;

let agentBrowser: nsIWindowlessBrowser | null = null;

function agentDocument(): Document {
  agentBrowser ??= Services.appShell.createWindowlessBrowser(true, 0);
  const doc = agentBrowser.document;
  if (!doc) throw new Error("windowless browser has no document");
  return doc;
}

/** Release the windowless browser used for `agent` parsing. */
export function closeAgentParser(): void {
  agentBrowser?.close();
  agentBrowser = null;
}

/** Parse `text` in `mode` and hand its top-level rules to `use`. */
export function withParsedRules<T>(
  text: string,
  mode: ParseMode,
  use: (rules: CSSRuleList) => T,
): T {
  if (mode === "chrome") {
    const sheet = new CSSStyleSheet();
    sheet.replaceSync(text);
    return use(sheet.cssRules);
  }
  const doc = agentDocument();
  const utils = doc.defaultView?.windowUtils;
  if (!utils) throw new Error("windowless document has no windowUtils");
  const url = `data:text/css;charset=utf-8,${encodeURIComponent(text)}`;
  utils.loadSheetUsingURIString(url, AGENT_SHEET);
  try {
    const sheet = InspectorUtils.getAllStyleSheets(doc, false).find((s) =>
      s.href === url
    ) as CSSStyleSheet | undefined;
    if (!sheet) throw new Error("AGENT sheet not found after loading it");
    return use(sheet.cssRules);
  } finally {
    utils.removeSheetUsingURIString(url, AGENT_SHEET);
  }
}

function collectProbes(rules: CSSRuleList, alive: Set<number>): void {
  for (const rule of Array.from(rules)) {
    const style = (rule as RuleWithStyle).style;
    if (style) {
      for (const prop of Array.from(style)) {
        if (prop.startsWith(PROBE)) alive.add(Number(prop.slice(PROBE.length)));
      }
    }
    const nested = (rule as RuleWithChildren).cssRules;
    if (nested) collectProbes(nested, alive);
  }
}

function probeBody(block: CssBlock, id: string | number): string {
  const decl = `${PROBE}${id}:1;`;
  if (block.kind === "style" || hasStyleAncestor(block)) return decl;
  return `:root{${decl}}`;
}

/** `inner` wrapped in the preludes of `block`'s ancestors. */
function wrapInAncestors(block: CssBlock, inner: string): string {
  const chain = ancestors(block);
  return chain.map((a) => `${a.prelude}{`).join("") + inner +
    "}".repeat(chain.length);
}

function namespaces(walked: CssWalk): string {
  return walked.statements
    .filter((s) => s.atName === "namespace")
    .map((s) => `${s.text};`)
    .join("\n");
}

function probeAlive(text: string, id: string, modes: ParseMode[]): boolean {
  return modes.some((mode) =>
    withParsedRules(text, mode, (rules) => {
      const alive = new Set<number>();
      collectProbes(rules, alive);
      return alive.has(Number(id));
    })
  );
}

/** First rule at nesting depth `depth` (0 = top level) or null. */
function ruleAtDepth(rules: CSSRuleList, depth: number): CSSRule | null {
  let list: CSSRuleList | undefined = rules;
  let rule: CSSRule | null = null;
  for (let d = 0; d <= depth; d++) {
    if (!list || list.length === 0) return null;
    rule = list[list.length - 1];
    list = (rule as RuleWithChildren).cssRules;
  }
  return rule;
}

export type DroppedRule = {
  line: number;
  kind: CssBlockKind;
  prelude: string;
  /** For a style rule: the selectors that fail on their own. */
  offenders: string[];
  detail?: string;
};

export type DropReport = {
  checked: number;
  styleRules: number;
  /** Preludes of every style rule that was checked. */
  selectors: string[];
  dropped: DroppedRule[];
};

/**
 * Parse `source` with a probe custom property inserted at the top of every
 * style rule and grouping at-rule, then report each block whose probe is not
 * in the CSSOM (only the outermost one when a whole subtree is lost). Leaf
 * at-rules are re-parsed one by one inside their ancestors; @keyframes also
 * compare the keyframe count. With several `modes`, a block counts as parsed
 * when any mode keeps it.
 */
export function findDroppedRules(
  source: string,
  modes: ParseMode[] = ["chrome"],
): DropReport {
  const walked = walkCss(source);
  const { text, blocks } = walked;
  let probed = "";
  let at = 0;
  for (const block of blocks) {
    if (block.kind === "leaf") continue;
    probed += text.slice(at, block.open + 1) + probeBody(block, block.id);
    at = block.open + 1;
  }
  probed += text.slice(at);

  const alive = new Set<number>();
  for (const mode of modes) {
    withParsedRules(probed, mode, (rules) => collectProbes(rules, alive));
  }

  const ns = namespaces(walked);
  const lost = new Set<number>();
  const dropped: DroppedRule[] = [];
  for (const block of blocks) {
    if (block.parent && lost.has(block.parent.id)) {
      lost.add(block.id);
      continue;
    }
    if (block.kind === "leaf") {
      const depth = ancestors(block).length;
      const own = `${block.prelude}${text.slice(block.open, block.close + 1)}`;
      const frames = block.atName === "keyframes"
        ? walkCss(text.slice(block.open + 1, block.close)).blocks
          .filter((b) => b.parent === null).length
        : -1;
      let detail = "";
      const kept = modes.some((mode) =>
        withParsedRules(
          `${ns}
${wrapInAncestors(block, own)}`,
          mode,
          (r) => {
            const rule = ruleAtDepth(r, depth);
            if (!rule || rule instanceof CSSNamespaceRule) return false;
            const parsed = (rule as RuleWithChildren).cssRules?.length ?? 0;
            if (frames >= 0 && parsed !== frames) {
              detail = `${frames - parsed} of ${frames} keyframes dropped`;
              return false;
            }
            return true;
          },
        )
      );
      if (!kept) {
        dropped.push({
          line: block.line,
          kind: block.kind,
          prelude: block.prelude,
          offenders: [],
          ...(detail ? { detail } : {}),
        });
      }
      continue;
    }
    if (alive.has(block.id)) continue;
    lost.add(block.id);
    const offenders: string[] = [];
    if (block.kind === "style") {
      const parts = splitSelectorList(block.prelude);
      if (parts.length > 1) {
        parts.forEach((part, n) => {
          const id = `9${n}${block.id}`;
          const body = `${part}{${PROBE}${id}:1;}`;
          const sheet = `${ns}\n${wrapInAncestors(block, body)}`;
          if (!probeAlive(sheet, id, modes)) {
            offenders.push(part);
          }
        });
      }
    }
    dropped.push({
      line: block.line,
      kind: block.kind,
      prelude: block.prelude,
      offenders,
    });
  }
  return {
    checked: blocks.length,
    styleRules: blocks.filter((b) => b.kind === "style").length,
    selectors: blocks.filter((b) => b.kind === "style").map((b) => b.prelude),
    dropped,
  };
}
