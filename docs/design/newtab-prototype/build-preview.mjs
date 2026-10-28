import { readFile, writeFile } from "node:fs/promises";

const output = new URL("./dist/", import.meta.url);
let html = await readFile(new URL("index.html", output), "utf8");
const script = html.match(/<script[^>]+src="([^"]+)"[^>]*><\/script>/);
const stylesheet = html.match(
  /<link[^>]+rel="stylesheet"[^>]+href="([^"]+)"[^>]*>/,
);
if (!script || !stylesheet)
  throw new Error("Expected a single Vite script and stylesheet");
const js = await readFile(new URL(script[1], output), "utf8");
const css = await readFile(new URL(stylesheet[1], output), "utf8");
html = html.replace(
  script[0],
  () =>
    `<script type="module">${js.replace(/<\/script/gi, "<\\/script")}</script>`,
);
html = html.replace(stylesheet[0], () => `<style>${css}</style>`);
const favicon = html.match(/<link[^>]+rel="icon"[^>]+href="([^"]+)"[^>]*>/);
if (favicon) {
  const svg = await readFile(new URL(favicon[1], output));
  html = html.replace(
    favicon[0],
    `<link rel="icon" href="data:image/svg+xml;base64,${svg.toString("base64")}" />`,
  );
}
await writeFile(new URL("preview.html", output), html);
console.log(
  `Standalone prototype: dist/preview.html (${(Buffer.byteLength(html) / 1024 / 1024).toFixed(1)} MiB)`,
);
