// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  discoverFeeds,
  inspectFeed,
  normaliseFeedUrl,
  parseFeed,
  sanitiseFeedContent,
} from "../FeedParser.sys.mts";
import type { SafeNode } from "#libs/newtab/types.ts";
import { decodeFeedResponse } from "../FeedNetwork.sys.mts";

const rss = (body: string) =>
  `<?xml version="1.0"?><rss version="2.0" xmlns:content="http://purl.org/rss/1.0/modules/content/"><channel><title>Feed &amp; News</title><link>https://example.com/</link>${body}</channel></rss>`;
const tags = (nodes: SafeNode[]): string[] =>
  nodes.flatMap((node) => [node.tag, ...tags(node.children ?? [])]);
async function rejects(work: () => Promise<unknown>, message: string) {
  let failed = false;
  try {
    await work();
  } catch {
    failed = true;
  }
  assert(failed, message);
}

async function testRssContentAndIdentity(): Promise<void> {
  const source = rss(
    `<item><guid>entry-1</guid><title>First</title><link>/story</link><pubDate>Tue, 01 Jan 2024 10:00:00 GMT</pubDate><content:encoded><![CDATA[<!DOCTYPE html><p>Hello <strong>world</strong><script>evil()</script><img src="/pixel.png" alt="Photo"><a href="javascript:alert(1)">bad</a><a href="/safe">good</a></p>]]></content:encoded></item>`,
  );
  const parsed = await parseFeed(source, "https://example.com/feed.xml");
  assertEquals(parsed.title, "Feed & News", "XML entities decode in titles");
  assertEquals(
    parsed.articles[0].sourceKey,
    "id:entry-1",
    "Publisher ID is used",
  );
  assertEquals(
    parsed.articles[0].link,
    "https://example.com/story",
    "Relative item links resolve",
  );
  assert(parsed.articles[0].hasFullContent, "content:encoded is preferred");
  assert(
    !JSON.stringify(parsed.articles[0].content).includes("evil()"),
    "Script contents are discarded",
  );
  assert(
    !JSON.stringify(parsed.articles[0].content).includes("javascript:"),
    "Active URLs are discarded",
  );
  assert(
    tags(parsed.articles[0].content).includes("img"),
    "Image becomes an explicit placeholder",
  );
  assertEquals(
    Object.values(parsed.articles[0].images)[0],
    "https://example.com/pixel.png",
    "Image source stays in parent storage only",
  );
  const updated = await parseFeed(
    source.replace("Hello", "Updated"),
    "https://example.com/feed.xml",
  );
  assertEquals(
    updated.articles[0].sourceKey,
    parsed.articles[0].sourceKey,
    "Body updates preserve identity",
  );
}
async function testAtomAndPlainText(): Promise<void> {
  const parsed = await parseFeed(
    `<feed xmlns="http://www.w3.org/2005/Atom" xml:base="https://example.com/news/"><title>Atom</title><link href="../"/><entry><id>urn:entry:1</id><title>Story</title><author><name>Writer</name></author><link href="story"/><content type="xhtml"><div xmlns="http://www.w3.org/1999/xhtml"><p>Text <em>emphasis</em></p></div></content></entry><entry><id>2</id><title>Literal</title><content type="text">&lt;img src="https://tracker.invalid/pixel"&gt;</content></entry><entry><id>3</id><title>External</title><content src="https://example.net/full"/><summary>Available summary</summary></entry></feed>`,
    "https://example.com/feed",
  );
  assertEquals(
    parsed.articles[0].link,
    "https://example.com/news/story",
    "xml:base resolves Atom link",
  );
  assertEquals(parsed.articles[0].author, "Writer", "Atom author is read");
  assert(
    tags(parsed.articles[0].content).includes("em"),
    "XHTML content retains allowed formatting",
  );
  assert(
    !tags(parsed.articles[1].content).includes("img"),
    "Plain text cannot become HTML",
  );
  assertEquals(
    Object.keys(parsed.articles[1].images).length,
    0,
    "Literal markup has no image requests",
  );
  assertEquals(
    parsed.articles[2].excerpt,
    "Available summary",
    "External Atom content is not retrieved",
  );
  assert(
    !parsed.articles[2].hasFullContent,
    "External content is not presented as a fetched body",
  );
}
async function testFallbackAndBadInputs(): Promise<void> {
  const a = await parseFeed(
    rss(`<item><title>No ID</title><description>old</description></item>`),
    "https://example.com/feed",
  );
  const b = await parseFeed(
    rss(`<item><title>No ID</title><description>new</description></item>`),
    "https://example.com/feed",
  );
  assertEquals(
    a.articles[0].sourceKey,
    b.articles[0].sourceKey,
    "Fallback identity excludes body text",
  );
  assertEquals(a.articles[0].publishedAt, null, "Missing date stays missing");
  assertEquals(a.articles[0].link, null, "An article URL is never invented");
  await rejects(
    () =>
      parseFeed(
        `<!DOCTYPE rss [<!ENTITY x SYSTEM "file:///etc/passwd">]>${rss("")}`,
        "https://example.com/feed",
      ),
    "External XML declarations are rejected",
  );
  await rejects(
    () => parseFeed("<rss version='2.0'><channel>", "https://example.com/feed"),
    "Invalid XML is rejected",
  );
  await rejects(
    () =>
      parseFeed(
        "<feed xmlns='http://purl.org/atom/ns#'/>",
        "https://example.com/feed",
      ),
    "Unsupported Atom format is rejected",
  );
  let rejected = false;
  try {
    normaliseFeedUrl("file:///secret");
  } catch {
    rejected = true;
  }
  assert(rejected, "Non-network subscription scheme is rejected");
  rejected = false;
  try {
    normaliseFeedUrl("https://user:secret@example.com/rss");
  } catch {
    rejected = true;
  }
  assert(rejected, "Embedded credentials are rejected");
  assertEquals(
    normaliseFeedUrl("https://example.com/feed?token=one&x=2#fragment"),
    "https://example.com/feed?token=one&x=2",
    "Query ordering is preserved",
  );
}
async function testDiscoveryAndSanitizer(): Promise<void> {
  const wrongMime = await inspectFeed(
    rss("<item><title>Still RSS</title></item>"),
    "https://example.com/feed",
    "text/html",
  );
  assertEquals(
    wrongMime.type,
    "feed",
    "RSS XML wins over an inaccurate HTML Content-Type",
  );
  const links = discoverFeeds(
    `<!-- <link rel="alternate" type="application/rss+xml" href="/hidden"> --><script>const s='<link rel="alternate" type="application/rss+xml" href="/script">';</script><base href="https://example.com/blog/"><link rel="alternate" type="application/rss+xml" title="News" href="feed.xml"><link rel="alternate" type="application/atom+xml" href="https://other.example/atom"><link rel="alternate" type="application/rss+xml" href="javascript:bad">`,
    "https://example.com/",
  );
  assertEquals(
    links.length,
    2,
    "Discovery excludes comments, scripts and unsafe links",
  );
  assertEquals(
    links[0].url,
    "https://example.com/blog/feed.xml",
    "Document base resolves candidate",
  );
  const safe = sanitiseFeedContent(
    `<p onclick="run()" style="background:url(https://tracker.invalid)">Hello&nbsp;&lt;script&gt;<svg><image href="https://tracker.invalid"/></svg><iframe src="https://tracker.invalid">hidden</iframe><img src="data:image/svg+xml,bad"><img src="https://images.example/photo.png"></p>`,
    "https://example.com/feed",
  );
  assert(
    !JSON.stringify(safe.content).includes("onclick"),
    "No arbitrary attributes reach AST",
  );
  assert(
    !JSON.stringify(safe.content).includes("tracker.invalid"),
    "Active embedded sources are discarded",
  );
  assertEquals(
    Object.keys(safe.images).length,
    1,
    "Only a safe explicit image is registered",
  );
  assert(
    !tags(safe.content).some((tag) =>
      ["script", "svg", "iframe", "style"].includes(tag)
    ),
    "AST stays within the closed element list",
  );
}
async function testDeclaredEncodings(): Promise<void> {
  const source = rss("<item><title>日本語</title></item>").replace(
    "Feed &amp; News",
    "日本語フィード",
  );
  const bytes = [0xff, 0xfe];
  for (let index = 0; index < source.length; index++) {
    const code = source.charCodeAt(index);
    bytes.push(code & 255, code >> 8);
  }
  const decoded = decodeFeedResponse({
    status: 200,
    url: "https://example.com/feed",
    bytes: Uint8Array.from(bytes),
    headers: {},
    contentType: "application/xml",
    charset: "",
  });
  assertEquals(
    (await parseFeed(decoded, "https://example.com/feed")).title,
    "日本語フィード",
    "UTF-16 BOM is decoded before XML parsing",
  );
  const latin = rss("<item><title>Café</title></item>").replace(
    'version="1.0"',
    'version="1.0" encoding="iso-8859-1"',
  );
  const latinBytes = Uint8Array.from(
    latin,
    (character) => character.charCodeAt(0),
  );
  const text = decodeFeedResponse({
    status: 200,
    url: "https://example.com/feed",
    bytes: latinBytes,
    headers: {},
    contentType: "application/xml",
    charset: "",
  });
  assertEquals(
    (await parseFeed(text, "https://example.com/feed")).articles[0].title,
    "Café",
    "Declared legacy encodings preserve text",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("FeedParser.test.mts", [
    {
      name: "UTF-16 and XML-declared legacy encodings",
      fn: testDeclaredEncodings,
    },
    {
      name: "RSS content, relative URLs and stable publisher identity",
      fn: testRssContentAndIdentity,
    },
    {
      name: "Atom XHTML, plain text and external content",
      fn: testAtomAndPlainText,
    },
    {
      name: "fallback IDs, malformed XML and URL restrictions",
      fn: testFallbackAndBadInputs,
    },
    {
      name: "discovery is inert and content becomes a closed AST",
      fn: testDiscoveryAndSanitizer,
    },
  ]);
}
