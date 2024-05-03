import * as fs from "node:fs/promises";
import * as path from "node:path";
import { ZSTDDecoder } from "zstddec";
import decompress from "decompress";
import fg from "fast-glob";
import autoprefixer from "autoprefixer";
import postcss from "postcss";
import postcssNested from "postcss-nested";
import postcssSorting from "postcss-sorting";
import chikodar from "chokidar";
import { build } from "vite";
import solidPlugin from "vite-plugin-solid";
import tsconfigPaths from "vite-tsconfig-paths";

import { injectManifest } from "./scripts/injectmanifest.js";
import { injectXHTML } from "./scripts/injectxhtml.js";

const r = (/** @type {string} */ dir) => {
  return path.resolve(import.meta.dirname, dir);
};

async function compile() {
  await build({
    root: r("src"),
    publicDir: r("public"),
    build: {
      sourcemap: true,
      reportCompressedSize: false,
      minify: false,
      cssMinify: false,
      emptyOutDir: true,
      assetsInlineLimit: 0,

      rollupOptions: {
        //https://github.com/vitejs/vite/discussions/14454
        preserveEntrySignatures: "allow-extension",
        input: {
          //index: "src/content/index.ts",
          startup: "src/components/startup/index.ts",
        },
        output: {
          esModule: true,
          entryFileNames: "content/[name].js",
        },
      },
      outDir: r("dist/noraneko"),

      assetsDir: "content/assets",
    },

    plugins: [
      tsconfigPaths(),
      solidPlugin({
        solid: {
          generate: "universal",
          moduleName: path.resolve(
            import.meta.dirname,
            "./src/components/solid-xul/solid-xul.ts",
          ),
        },
      }),
    ],
    resolve: {
      alias: [{ find: "@content", replacement: r("src/content") }],
    },
  });
  const entries = await fg("./src/skin/**/*");

  for (const _entry of entries) {
    const entry = _entry.replaceAll("\\", "/");
    const stat = await fs.stat(entry);
    if (stat.isFile()) {
      if (entry.endsWith(".pcss")) {
        // file that postcss process required
        const result = await postcss([
          autoprefixer,
          postcssNested,
          postcssSorting,
        ]).process((await fs.readFile(entry)).toString(), {
          from: entry,
          to: entry.replace("src/", "dist/").replace(".pcss", ".css"),
        });

        await fs.mkdir(path.dirname(entry).replace("src/", "dist/noraneko/"), {
          recursive: true,
        });
        await fs.writeFile(
          entry.replace("src/", "dist/noraneko/").replace(".pcss", ".css"),
          result.css,
        );
        if (result.map)
          await fs.writeFile(
            `${entry
              .replace("src/", "dist/noraneko/")
              .replace(".pcss", ".css")}.map`,
            result.map.toString(),
          );
      } else {
        // normal file
        await fs.cp(entry, entry.replace("src/", "dist/noraneko/"));
      }
    }
  }

  // await fs.cp("public", "dist", { recursive: true });
}

//import { remote } from "webdriverio";
import puppeteer from "puppeteer-core";
import { exit } from "node:process";
import { Browser } from "puppeteer-core";

const VERSION = "000";

async function run() {
  await compile();
  try {
    await fs.access("dist/bin");
    await fs.access("dist/bin/nora.version.txt");
    if (
      VERSION !== (await fs.readFile("dist/bin/nora.version.txt")).toString()
    ) {
      await fs.rm("dist/bin", { recursive: true });
      await fs.mkdir("dist/bin", { recursive: true });
      throw "have to decompress";
    }
  } catch {
    console.log("decompressing bin.tzst");
    const decoder = new ZSTDDecoder();
    await decoder.init();
    const archive = Buffer.from(
      decoder.decode(await fs.readFile("bin.tar.zst")),
    );

    await decompress(archive, "dist/bin");

    console.log("decompress complete!");
    await fs.writeFile("dist/bin/nora.version.txt", VERSION);
  }

  console.log("inject");
  await injectManifest();
  await injectXHTML();
  //await injectUserJS(`noraneko${VERSION}`);

  try {
    await fs.access("./profile/test");

    try {
      await fs.access("dist/profile/test");
      await fs.rm("dist/profile/test");
    } catch {}
    // https://searchfox.org/mozilla-central/rev/b4a96f411074560c4f9479765835fa81938d341c/toolkit/xre/nsAppRunner.cpp#1514
    // 可能性はある、まだ必要はない
  } catch {}

  /** @type Browser | null */
  let browser = null;
  let watch_running = false;

  let intended_close = false;

  const watcher = chikodar
    .watch("src", { persistent: true })
    .on("all", async (ev, path, stat) => {
      if (watch_running) return;
      watch_running = true;
      if (browser) {
        console.log("Browser Restarting...");
        intended_close = true;
        await browser.close();

        watcher.close();
        run();
      }
      watch_running = false;
    });

  const isWin = process.platform === "win32";

  browser = await puppeteer.launch({
    headless: false,
    protocol: "webDriverBiDi",
    dumpio: true,
    product: "firefox",
    executablePath: isWin ? "dist/bin/firefox.exe" : "dist/bin/firefox",
    args: ["--profile dist/profile/test"],
  });

  browser.on("disconnected", () => {
    if (!intended_close) exit();
  });
}
// run
if (process.argv[2] && process.argv[2] === "--run") {
  run();
} else {
  compile();
}
