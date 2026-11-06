// SPDX-License-Identifier: MPL-2.0
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
) as {
  setTimeout: (callback: () => void, milliseconds: number) => number;
};
import type { SafeNode, SafeTag } from "#libs/newtab/types.ts";
import type { ParsedArticle, ParsedFeed } from "./types.ts";

const MAX_XML_NODES = 60_000;
const MAX_CONTENT_NODES = 20_000;
const MAX_DEPTH = 64;
const ALLOWED = new Set<SafeTag>([
  "p",
  "h1",
  "h2",
  "h3",
  "h4",
  "ul",
  "ol",
  "li",
  "strong",
  "em",
  "blockquote",
  "pre",
  "code",
  "br",
  "hr",
  "a",
]);
const VOID = new Set([
  "br",
  "hr",
  "img",
  "input",
  "meta",
  "link",
  "source",
  "area",
  "base",
  "embed",
  "wbr",
]);
const DROP_CONTENT = new Set([
  "script",
  "style",
  "iframe",
  "object",
  "svg",
  "math",
  "video",
  "audio",
  "form",
  "template",
  "noscript",
  "canvas",
]);

export function normaliseFeedUrl(value: string, base?: string): string {
  if (value.length > 8192) throw new Error("Feed URL is too long.");
  const url = base ? new URL(value, base) : new URL(value);
  if (!/^https?:$/.test(url.protocol) || url.username || url.password) {
    throw new Error("Use an HTTP or HTTPS URL without embedded credentials.");
  }
  url.hash = "";
  return url.href;
}

function safeUrl(value: string | null, base: string): string | null {
  if (!value?.trim()) return null;
  try {
    return normaliseFeedUrl(value.trim(), base);
  } catch {
    return null;
  }
}

export async function stableHash(value: string): Promise<string> {
  const encoded = new TextEncoder().encode(value);
  const bytes = await crypto.subtle.digest(
    "SHA-256",
    encoded.buffer as ArrayBuffer,
  );
  return Array.from(
    new Uint8Array(bytes),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

/** Entity decoding only: literal '<'/'>' are escaped before an inert text document
 * is parsed, so no input can create a tag or issue a resource request. */
function decodeText(value: string): string {
  if (!value.includes("&")) return value.replaceAll("\0", "");
  const text = value.replaceAll("<", "&lt;").replaceAll(">", "&gt;");
  const document = new DOMParser().parseFromString(
    `<body>${text}</body>`,
    "text/html",
  );
  return (document.body?.textContent ?? "").replaceAll("\0", "");
}

function attributes(token: string): Map<string, string> {
  const result = new Map<string, string>();
  const body = token.replace(/^<\/?[\w:-]+/, "").replace(/\/?\s*>$/, "");
  const pattern = /([^\s=/>]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g;
  for (const match of body.matchAll(pattern)) {
    const name = match[1].toLowerCase();
    if (!result.has(name)) {
      result.set(name, decodeText(match[2] ?? match[3] ?? match[4] ?? ""));
    }
  }
  return result;
}

/** A deliberately small HTML tokenizer creates a closed AST. It never inserts
 * network content into a DOM; unknown markup is flattened, active markup dropped. */
export function sanitiseFeedContent(
  source: string,
  base: string,
): { content: SafeNode[]; images: Record<string, string>; text: string } {
  const content: SafeNode[] = [];
  const images: Record<string, string> = {};
  const stack: { name: string; children: SafeNode[] }[] = [{
    name: "root",
    children: content,
  }];
  const ignored: string[] = [];
  let count = 0;
  let imageCount = 0;
  let position = 0;
  const pushText = (value: string) => {
    if (!value || ignored.length) return;
    if (++count > MAX_CONTENT_NODES) {
      throw new Error("Feed content has too many elements.");
    }
    const pre = stack.some((entry) =>
      entry.name === "pre" || entry.name === "code"
    );
    const text = pre
      ? decodeText(value)
      : decodeText(value).replace(/\s+/g, " ");
    if (text) stack[stack.length - 1].children.push({ tag: "text", text });
  };
  while (position < source.length) {
    const start = source.indexOf("<", position);
    if (start < 0) {
      pushText(source.slice(position));
      break;
    }
    pushText(source.slice(position, start));
    if (source.startsWith("<!--", start)) {
      const end = source.indexOf("-->", start + 4);
      position = end < 0 ? source.length : end + 3;
      continue;
    }
    let end = start + 1;
    let quote = "";
    for (; end < source.length; end++) {
      const character = source[end];
      if (quote) { if (character === quote) quote = ""; }
      else if (character === '"' || character === "'") quote = character;
      else if (character === ">") break;
    }
    if (end >= source.length) {
      pushText(source.slice(start));
      break;
    }
    const token = source.slice(start, end + 1);
    position = end + 1;
    const match = token.match(/^<\s*(\/?)\s*([a-zA-Z][\w:-]*)/);
    if (!match) continue;
    const closing = Boolean(match[1]);
    const name = match[2].toLowerCase();
    if (closing) {
      if (ignored.length) {
        const index = ignored.lastIndexOf(name);
        if (index >= 0) ignored.splice(index);
        continue;
      }
      for (let index = stack.length - 1; index > 0; index--) {
        if (stack[index].name === name) {
          stack.splice(index);
          break;
        }
      }
      continue;
    }
    if (DROP_CONTENT.has(name)) {
      if (!token.endsWith("/>")) ignored.push(name);
      if (ignored.length > MAX_DEPTH) {
        throw new Error("Feed content is too deeply nested.");
      }
      continue;
    }
    if (ignored.length) continue;
    if (++count > MAX_CONTENT_NODES) {
      throw new Error("Feed content has too many elements.");
    }
    const attrs = attributes(token);
    if (name === "img") {
      const url = safeUrl(attrs.get("src") ?? null, base);
      if (url) {
        const imageId = `image-${imageCount++}`;
        images[imageId] = url;
        stack[stack.length - 1].children.push({
          tag: "img",
          imageId,
          alt: (attrs.get("alt") ?? "").slice(0, 1000),
        });
      }
      continue;
    }
    const alias = name === "div" || name === "section" || name === "article"
      ? "p"
      : name === "b"
      ? "strong"
      : name === "i"
      ? "em"
      : name;
    if (!ALLOWED.has(alias as SafeTag)) continue;
    const node: SafeNode = {
      tag: alias === "h1" ? "h2" : alias as SafeTag,
      children: [],
    };
    if (alias === "a") {
      const href = safeUrl(attrs.get("href") ?? null, base);
      if (href) node.href = href;
      else node.tag = "p";
    }
    stack[stack.length - 1].children.push(node);
    if (!VOID.has(name) && !token.endsWith("/>")) {
      if (stack.length >= MAX_DEPTH) {
        throw new Error("Feed content is too deeply nested.");
      }
      stack.push({ name, children: node.children! });
    }
  }
  const text = (nodes: SafeNode[]): string =>
    nodes.map((node) =>
      node.tag === "text"
        ? node.text ?? ""
        : node.tag === "img"
        ? node.alt ?? ""
        : `${text(node.children ?? [])} `
    ).join("");
  return { content, images, text: text(content).replace(/\s+/g, " ").trim() };
}

function child(
  element: Element,
  name: string,
  namespace?: string,
): Element | null {
  return Array.from(element.children).find((node) =>
    node.localName.toLowerCase() === name &&
    (!namespace || node.namespaceURI === namespace)
  ) ?? null;
}
function children(element: Element, name: string): Element[] {
  return Array.from(element.children).filter((node) =>
    node.localName.toLowerCase() === name
  );
}
function value(element: Element, name: string): string {
  return child(element, name)?.textContent?.trim() ?? "";
}
function baseFor(element: Element, fallback: string): string {
  const bases: string[] = [];
  for (
    let current: Element | null = element;
    current;
    current = current.parentElement
  ) {
    const base = current.getAttributeNS(
      "http://www.w3.org/XML/1998/namespace",
      "base",
    );
    if (base) bases.unshift(base);
  }
  let result = fallback;
  for (const base of bases) result = safeUrl(base, result) ?? result;
  return result;
}
function atomLink(element: Element, base: string): string | null {
  const links = children(element, "link");
  const link = links.find((node) =>
    !node.getAttribute("rel") || node.getAttribute("rel") === "alternate"
  );
  return safeUrl(link?.getAttribute("href") ?? null, base);
}
function contentMarkup(element: Element | null): string {
  if (!element) return "";
  if (element.getAttribute("type") === "xhtml") {
    return Array.from(element.childNodes).map((node) =>
      new XMLSerializer().serializeToString(node)
    ).join("");
  }
  return element.textContent ?? "";
}

export function discoverFeeds(
  html: string,
  base: string,
): { title: string; url: string }[] {
  const candidates: { title: string; url: string }[] = [];
  const seen = new Set<string>();
  // Tokenize without creating a DOM, excluding comments and active-element text.
  let ignored: string | null = null;
  let documentBase = base;
  let baseChosen = false;
  const tags = html.matchAll(
    /<!--[\s\S]*?-->|<\/?[a-zA-Z][\w:-]*(?:[^>"']|"[^"]*"|'[^']*')*>/g,
  );
  let count = 0;
  for (const [token] of tags) {
    if (++count > MAX_XML_NODES) {
      throw new Error("The page contains too many elements.");
    }
    if (token.startsWith("<!--")) continue;
    const match = token.match(/^<(\/?)([\w:-]+)/);
    if (!match) continue;
    const name = match[2].toLowerCase();
    if (ignored) {
      if (match[1] && name === ignored) ignored = null;
      continue;
    }
    if (match[1]) continue;
    if (DROP_CONTENT.has(name)) {
      ignored = name;
      continue;
    }
    const attrs = attributes(token);
    if (name === "base" && !baseChosen) {
      documentBase = safeUrl(attrs.get("href") ?? null, base) ?? base;
      baseChosen = true;
    }
    if (name !== "link") continue;
    if (
      !(attrs.get("rel") ?? "").toLowerCase().split(/\s+/).includes("alternate")
    ) continue;
    if (
      !/^application\/(?:rss\+xml|atom\+xml)$/i.test(attrs.get("type") ?? "")
    ) continue;
    const url = safeUrl(attrs.get("href") ?? null, documentBase);
    if (url && !seen.has(url)) {
      seen.add(url);
      candidates.push({
        title: (attrs.get("title") ?? url).slice(0, 1000),
        url,
      });
    }
    if (candidates.length >= 20) break;
  }
  return candidates;
}

export async function parseFeed(
  source: string,
  sourceUrl: string,
  now = Date.now(),
): Promise<ParsedFeed> {
  if (
    source.length > 5 * 1024 * 1024 ||
    (source.match(/<[a-zA-Z_]/g)?.length ?? 0) > MAX_XML_NODES
  ) throw new Error("The feed exceeds the parsing limit.");
  // A doctype inside CDATA is article text, not an XML external declaration.
  // Remove only lexical CDATA/comments for this pre-parse check; their content
  // is later processed by the inert AST tokenizer, never a document loader.
  const declarations = source.replace(
    /<!\[CDATA\[[\s\S]*?\]\]>|<!--[\s\S]*?-->/g,
    "",
  );
  if (
    /<!\s*(?:DOCTYPE|ENTITY)\b/i.test(declarations) ||
    /<\?(?!xml\s)[\s\S]*?\?>/i.test(declarations)
  ) throw new Error("External XML declarations are not supported.");
  const document = new DOMParser().parseFromString(source, "application/xml");
  if (
    document.getElementsByTagName("parsererror").length ||
    !document.documentElement
  ) throw new Error("The feed contains invalid XML.");
  const root = document.documentElement;
  const queue: { node: Element; depth: number }[] = [{ node: root, depth: 0 }];
  for (let index = 0; index < queue.length; index++) {
    const { node, depth } = queue[index];
    if (depth > MAX_DEPTH || queue.length > MAX_XML_NODES) {
      throw new Error("The feed has too many nested elements.");
    }
    for (const element of node.children) {
      queue.push({ node: element, depth: depth + 1 });
    }
  }
  const atom = root.localName === "feed" &&
    (!root.namespaceURI || root.namespaceURI === "http://www.w3.org/2005/Atom");
  const channel = root.localName === "rss" &&
      /^2(?:\.|$)/.test(root.getAttribute("version") ?? "")
    ? child(root, "channel")
    : null;
  if (!atom && !channel) {
    throw new Error("Only RSS 2.0 and Atom 1.0 feeds are supported.");
  }
  const container = channel ?? root;
  const feedBase = baseFor(container, sourceUrl);
  const siteUrl = atom
    ? atomLink(container, feedBase)
    : safeUrl(value(container, "link"), feedBase);
  const items = children(container, atom ? "entry" : "item");
  if (items.length > 2000) {
    throw new Error("The feed contains too many articles.");
  }
  const articles: ParsedArticle[] = [];
  const fallbacks = new Map<string, number>();
  for (const item of items) {
    const base = baseFor(item, feedBase);
    const title = (value(item, "title") || "Untitled").slice(0, 2000);
    const permalink = child(item, "guid");
    const link = atom
      ? atomLink(item, base)
      : safeUrl(value(item, "link"), base) ??
        (permalink?.getAttribute("isPermaLink") !== "false"
          ? safeUrl(permalink?.textContent ?? null, base)
          : null);
    const author = (atom
      ? value(child(item, "author") ?? item, "name")
      : value(item, "creator") || value(item, "author")).slice(0, 1000);
    const rawDate = atom
      ? value(item, "published") || value(item, "updated")
      : value(item, "pubdate") || value(item, "date");
    const parsedDate = Date.parse(rawDate);
    const publishedAt =
      Number.isFinite(parsedDate) && parsedDate <= now + 86400000
        ? parsedDate
        : null;
    const encoded = child(
      item,
      "encoded",
      "http://purl.org/rss/1.0/modules/content/",
    );
    const primary = atom ? child(item, "content") : encoded;
    const summary = child(item, atom ? "summary" : "description");
    // Atom src points at external content: it is never a request to fetch it.
    const bodyElement = primary?.hasAttribute("src")
      ? summary
      : primary ?? summary;
    let markup = contentMarkup(bodyElement);
    if (
      atom && bodyElement &&
      !["html", "xhtml"].includes(bodyElement.getAttribute("type") ?? "text")
    ) {
      markup = markup.replaceAll("&", "&amp;").replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;");
    }
    const parsed = sanitiseFeedContent(markup, base);
    const explicitId = value(item, atom ? "id" : "guid");
    let sourceKey = explicitId
      ? `id:${explicitId}`
      : link
      ? `link:${link}`
      : `fallback:${await stableHash(
        JSON.stringify([title, rawDate, author]),
      )}`;
    if (!explicitId && !link) {
      const occurrence = fallbacks.get(sourceKey) ?? 0;
      fallbacks.set(sourceKey, occurrence + 1);
      sourceKey += `:${occurrence}`;
    }
    articles.push({
      sourceKey,
      title,
      link,
      author,
      publishedAt,
      excerpt: parsed.text.slice(0, 320),
      hasFullContent: Boolean(primary && !primary.hasAttribute("src")),
      content: parsed.content,
      images: parsed.images,
    });
    if (articles.length % 25 === 0) {
      await new Promise<void>((resolve) =>
        setTimeout(resolve, 0)
      );
    }
  }
  return {
    title: (value(container, "title") || new URL(sourceUrl).hostname).slice(
      0,
      2000,
    ),
    siteUrl,
    articles,
  };
}

/** Publishers sometimes serve RSS as text/html. The actual supported XML
 * format takes precedence; HTML discovery is only a fallback, never scraping. */
export async function inspectFeed(
  source: string,
  url: string,
  contentType: string,
): Promise<
  { type: "feed"; feed: ParsedFeed } | {
    type: "discovery";
    candidates: { title: string; url: string }[];
  }
> {
  try {
    return { type: "feed", feed: await parseFeed(source, url) };
  } catch (cause) {
    if (
      !/html/i.test(contentType) &&
      !/^\s*(?:<!--[^]*?-->\s*)*(?:<!doctype\s+html|<html\b|<head\b|<body\b)/i
        .test(source)
    ) throw cause;
    const candidates = discoverFeeds(source, url);
    if (!candidates.length) {
      throw new Error("No RSS or Atom feed links were found on this page.");
    }
    return { type: "discovery", candidates };
  }
}
