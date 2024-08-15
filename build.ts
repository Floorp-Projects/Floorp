import * as fs from "node:fs/promises";
import * as path from "node:path";
import { injectManifest } from "./scripts/inject/manifest.js";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.js";
import { applyMixin } from "./scripts/inject/mixin-loader.js";
import { $, type ResultPromise, type Result } from "execa";
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
const binDir = "_dist/bin";

try {
  await fs.access("dist");
  await fs.rename("dist", "_dist");
} catch {}

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

    await decompress("nora-bin-tmp.tar", "./_dist/bin");
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
  console.log("initBin");
  if (!hasBin) {
    console.log("There seems no bin. decompressing.");
    await fs.mkdir(binDir, { recursive: true });
    await decompressBin();
  }
}

let devProcesses: ResultPromise<{
  cwd: string;
  stdout: "inherit";
  stdin: "ignore";
}>[];
let devInit = false;

async function run(mode: "dev" | "test" = "dev") {
  await initBin();
  if (!devInit) {
    console.log("run dev servers");
    devProcesses = [
      $({
        cwd: r("./apps/main"),
        stdin: "ignore",
        stdout: "inherit",
      })`pnpm vite dev`,
      $({
        cwd: r("./apps/designs"),
        stdin: "ignore",
        stdout: "inherit",
      })`pnpm vite dev`,
    ];
    if (mode === "test") {
      devProcesses.push(
        $({
          cwd: r("./apps/test"),
          stdin: "ignore",
          stdout: "inherit",
        })`pnpm run run`,
      );
    }
    devInit = true;
  }
  await Promise.all([
    $({ cwd: r("./apps/startup") })`pnpm vite build --mode ${mode}`,

    injectManifest("_dist/bin"),
    (async () => {
      await injectXHTML("_dist/bin");
      await injectXHTMLDev("_dist/bin");
    })(),
    applyMixin("_dist/bin"),
  ]);

  //await injectUserJS(`noraneko${VERSION}`);

  try {
    await fs.access("./profile/test");

    try {
      await fs.access("_dist/profile/test");
      await fs.rm("_dist/profile/test");
    } catch {}
    // https://searchfox.org/mozilla-central/rev/b4a96f411074560c4f9479765835fa81938d341c/toolkit/xre/nsAppRunner.cpp#1514
    // 可能性はある、まだ必要はない
  } catch {}

  let browser: Browser | null = null;
  //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await fs.mkdir("./_dist/profile/test", { recursive: true });

  browser = await puppeteer.launch({
    headless: false,
    protocol: "webDriverBiDi",
    dumpio: true,
    browser: "firefox",
    executablePath: binPathExe,
    userDataDir: "./_dist/profile/test",
    args: ["-jsdebugger"],
    extraPrefsFirefox: {
      "browser.newtabpage.enabled": true,
      //https://searchfox.org/mozilla-central/rev/02a4a649ed75ebaf3fbdf301c3d3137baf6842a1/devtools/shared/security/auth.js#170
      "devtools.debugger.prompt-connection": false,

      //? Thank you for `arai` san in Mozilla!
      //? This pref allows to run import of http(s) protocol in browser-top or about: pages
      //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/dom/security/nsContentSecurityManager.cpp#1037-1041
      //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/modules/libpref/init/StaticPrefList.yaml#15063
      "security.disallow_privileged_https_script_loads": false,
      //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/dom/security/nsContentSecurityUtils.cpp#1600-1607
      //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/dom/security/nsContentSecurityUtils.cpp#1445-1450
      //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/modules/libpref/init/StaticPrefList.yaml#14743
      "security.allow_parent_unrestricted_js_loads": true,

      //https://searchfox.org/mozilla-central/rev/71aada9d4055e420f91f3d0fa107f0328763e40b/browser/app/profile/firefox.js#1249
      "browser.preferences.moreFromMozilla": false,
    },
    defaultViewport: { height: 0, width: 0 },
    timeout: 0,
  });

  if (mode === "dev") {
    const pages = await browser.pages();
    await pages[0].goto("about:newtab", {
      timeout: 0,
      waitUntil: "domcontentloaded",
    });
    //? We should not go to about:preferences
    //? Puppeteer cannot get the load event so when reload, this errors.
    // await pages[0].goto("about:preferences", {
    //   timeout: 0,
    //   waitUntil: "domcontentloaded",
    // });
  } else if (mode === "test") {
    browser.pages().then(async (page) => {
      await page[0].goto("about:newtab", {
        timeout: 0,
        waitUntil: "domcontentloaded",
      });
      // await page[0].goto("about:preferences#csk", {
      //   timeout: 0,
      //   waitUntil: "domcontentloaded",
      // });
    });
  }

  browser.on("disconnected", () => {
    process.exit();
  });
}

async function build() {
  const binPath = "../obj-x86_64-pc-windows-msvc/dist/bin";
  await Promise.all([
    $({ cwd: r("./apps/startup") })`pnpm vite build`,
    $({ cwd: r("./apps/main") })`pnpm vite build`,
    $({ cwd: r("./apps/designs") })`pnpm vite build`,

    injectXHTML(binPath),

    applyMixin(binPath),
  ]);
  await injectManifest(binPath);
}

if (process.argv[2]) {
  switch (process.argv[2]) {
    case "--run":
      run();
      break;
    case "--test":
      run("test");
      break;
    case "--build":
      build();
      break;
  }
}
