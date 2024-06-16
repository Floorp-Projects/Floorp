import * as fs from "node:fs/promises";
import * as path from "node:path";
import fg from "fast-glob";
import chokidar from "chokidar";
import { build } from "vite";
import solidPlugin from "vite-plugin-solid";
import tsconfigPaths from "vite-tsconfig-paths";
import { injectManifest } from "./scripts/inject/manifest.js";
import { injectXHTML } from "./scripts/inject/xhtml.js";
import { injectJavascript } from "./scripts/inject/javascript.js";
import { $ } from "execa";
import decompress from "decompress";
import puppeteer, { type Browser } from "puppeteer-core";

//? when the linux binary has published, I'll sync linux bin version
const VERSION = process.platform === "win32" ? "001" : "000";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

const isExists = async (path: string) => {
  return await fs
    .access(path)
    .then(() => true)
    .catch(() => false);
};

const binTar =
  process.platform === "win32" ? "nora-win_x64-bin.tar.zst" : "bin.tar.zst";
const binDir = "dist/bin";
const binPath = path.join(
  binDir,
  process.platform === "win32" ? "noraneko" : "firefox",
);
const binPathExe = binPath + (process.platform === "win32" ? ".exe" : "");

const binVersion = path.join(binDir, "nora.version.txt");

async function decompressBin() {
  try {
    console.log(`decompressing ${binTar}`);
    if (!(await isExists(binTar))) {
      console.error(`${binTar} not found`);
      process.exit(1);
    }
    if (
      !(await $({ stdin: "ignore" })`zstd -v`).stderr.includes("Zstandard CLI")
    ) {
      //zstd not installed
      console.error(`Please install zstd for decompressing ${binTar}`);
      process.exit();
    } else {
      await $`zstd -d nora-win_x64-bin.tar.zst -o nora-bin-tmp.tar`;
    }

    await decompress("nora-bin-tmp.tar", "./dist/bin");
    await fs.rm("nora-bin-tmp.tar");
    //fs.readFile(binTar);
    console.log("decompress complete!");
    await fs.writeFile(binVersion, VERSION);
  } catch (e) {
    console.error(e);
    process.exit(1);
  }
}

async function initBin() {
  const hasVersion = await isExists(binVersion);
  const hasBin = await isExists(binPathExe);

  if (hasVersion) {
    const version = (await fs.readFile(binVersion)).toString();
    const mismatch = VERSION !== version;
    if (mismatch) {
      console.log(`version mismatch ${version} !== ${VERSION}`);
      await fs.rm(binDir, { recursive: true });
      await fs.mkdir(binDir, { recursive: true });
      await decompressBin();
      return;
    }
  } else {
    if (hasBin) {
      console.log(`bin exists, but version file not found, writing ${VERSION}`);
      await fs.mkdir(binDir, { recursive: true });
      await fs.writeFile(binVersion, VERSION);
    }
  }
  if (!hasBin) {
    console.log("There seems no bin. decompressing.");
    await fs.mkdir(binDir, { recursive: true });
    await decompressBin();
  }
}

async function compile() {
  await build({
    root: r("src"),
    publicDir: r("src/public"),
    build: {
      sourcemap: true,
      reportCompressedSize: false,
      minify: false,
      cssMinify: false,
      emptyOutDir: true,
      assetsInlineLimit: 0,
      modulePreload: false,

      rollupOptions: {
        //https://github.com/vitejs/vite/discussions/14454
        preserveEntrySignatures: "allow-extension",
        input: {
          //index: "src/content/index.ts",
          startupBrowser: "src/components/startup/browser/index.ts",
          startupPreferences: "src/components/startup/preferences/index.ts",
        },
        output: {
          esModule: true,
          entryFileNames: "content/[name].js",
        },
      },
      outDir: r("dist/noraneko"),

      assetsDir: "content/assets",
    },
    css: {
      transformer: "lightningcss",
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
}

async function run() {
  await compile();
  await initBin();
  console.log("inject");
  await injectManifest();
  await injectXHTML();
  await injectJavascript();

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

  let browser: Browser | null = null;
  let watch_running = false;

  let intended_close = false;

  const watcher = chokidar
    .watch("src", { persistent: true })
    .on("all", async () => {
      if (watch_running) return;
      watch_running = true;
      if (browser) {
        console.log("Browser Restarting...");
        intended_close = true;
        await browser.close();

        await watcher.close();
        await run();
      }
      watch_running = false;
    });

  //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await fs.mkdir("./dist/profile/test", { recursive: true });

  browser = await puppeteer.launch({
    headless: false,
    protocol: "webDriverBiDi",
    dumpio: true,
    product: "firefox",
    executablePath: binPathExe,
    userDataDir: "./dist/profile/test",
    extraPrefsFirefox: { "browser.newtabpage.enabled": true },
    defaultViewport: { height: 0, width: 0 },
  });

  await (await browser.pages())[0].goto("about:newtab");

  const page = await browser.newPage();
  await page.goto("about:preferences");

  browser.on("disconnected", () => {
    if (!intended_close) process.exit();
  });
}

// run
if (process.argv[2] && process.argv[2] === "--run") {
  run();
} else {
  compile();
}
