import * as esbuild from "esbuild";
import { $ } from "execa";
import * as fs from "node:fs/promises";
import * as path from "node:path";
import { ZSTDDecoder } from "zstddec";
import decompress from "decompress";

import { injectManifest } from "./scripts/injectmanifest.js";
import { injectXHTML } from "./scripts/injectxhtml.js";

await esbuild.build({
  entryPoints: ["src/content/index.ts"],
  bundle: true,
  sourcemap: true,
  splitting: true,
  minify: false,
  format: "esm",
  outdir: "dist/noraneko/content",
  target: "es2020",
});

import fg from "fast-glob";

import autoprefixer from "autoprefixer";
import postcss from "postcss";
import postcssNested from "postcss-nested";
import postcssSorting from "postcss-sorting";
import { Readable } from "node:stream";
import { exit } from "node:process";

const entries = await fg("./src/skin/**/*");

for (const _entry of entries) {
  const entry = _entry.replaceAll("\\", "/");
  const stat = await fs.stat(entry);
  if (stat.isFile) {
    if (entry.endsWith(".pcss")) {
      const result = await postcss([autoprefixer, postcssNested, postcssSorting]).process((await fs.readFile(entry)).toString(), { from: entry, to: entry.replace("src/", "dist/").replace(".pcss", ".css") });

      await fs.mkdir(path.dirname(entry).replace("src/", "dist/noraneko/"), { recursive: true });
      await fs.writeFile(entry.replace("src/", "dist/noraneko/").replace(".pcss", ".css"), result.css);
      if (result.map) await fs.writeFile(`${entry.replace("src/", "dist/noraneko/").replace(".pcss", ".css")}.map`, result.map.toString());
    } else {
      await fs.cp(entry, entry.replace("src/", "dist/noraneko/"));
    }
  }
}

await fs.cp("public", "dist", { recursive: true });

const VERSION = "000";
// run
if (process.argv[2] && process.argv[2] === "--run") {
  try {
    await fs.access("dist/bin");
    await fs.access("dist/bin/nora.version.txt");
    if (VERSION !== (await fs.readFile("dist/bin/nora.version.txt")).toString()) {
      await fs.rm("dist/bin", { recursive: true });
      await fs.mkdir("dist/bin", { recursive: true });
      throw "have to decompress";
    }
  } catch {
    console.log("decompressing bin.tzst");
    const decoder = new ZSTDDecoder();
    await decoder.init();
    const archive = Buffer.from(decoder.decode(await fs.readFile("bin.tar.zst")));

    await decompress(archive, "dist/bin");

    console.log("decompress complete!");
    await fs.writeFile("dist/bin/nora.version.txt", VERSION);
  }

  //await fs.cp("dist", "bin/chrome/noraneko/content", { recursive: true });

  console.log("inject");
  await injectManifest();
  await injectXHTML();
  //await injectUserJS(`noraneko${VERSION}`);

  try {
    await fs.access("./profile/test");
    await fs.rm("./profile/test", { recursive: true });
  } catch {}

  console.log("Running Firefox");
  await $({
    stdio: "inherit",
  })`./dist/bin/firefox.exe -no-remote -wait-for-browser -attach-console --profile ./dist/profile/test`;
}
