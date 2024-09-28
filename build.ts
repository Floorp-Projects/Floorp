import * as fs from "node:fs/promises";
import * as path from "node:path";
import { injectManifest } from "./scripts/inject/manifest.js";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.js";
import { applyMixin } from "./scripts/inject/mixin-loader.js";
import puppeteer, { type Browser } from "puppeteer-core";
import {
  createServer,
  type ViteDevServer,
  build as buildVite,
  build,
} from "vite";
import AdmZip from "adm-zip";
import { execa } from "execa";
import { cwd } from "node:process";
import { runBrowser } from "./scripts/launchBrowser/index.js";
import { savePrefsForProfile } from "./scripts/launchBrowser/savePrefs.js";

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

const binArchive =
  process.platform === "win32" ? "noraneko-win-amd64-dev.zip" : "bin.tar.zst";
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
    console.log(`decompressing ${binArchive}`);
    if (!(await isExists(binArchive))) {
      console.error(`${binArchive} not found`);
      process.exit(1);
    }
    // if (
    //   !(await $({ stdin: "ignore" })`zstd -v`).stderr.includes("Zstandard CLI")
    // ) {
    //   //zstd not installed
    //   console.error(`Please install zstd for decompressing ${binTar}`);
    //   process.exit();
    // } else {
    //   await $`zstd -d nora-win_x64-bin.tar.zst -o nora-bin-tmp.tar`;
    // }
    new AdmZip(binArchive).extractAllTo(binDir);
    // await decompress("nora-bin-tmp.tar", "./_dist/bin");
    // await fs.rm("nora-bin-tmp.tar");
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

let devViteProcesses: ViteDevServer[];
let buildViteProcesses: any[];
const devExecaProcesses: unknown[] = [];
let devInit = false;

async function run(mode: "dev" | "test" = "dev") {
  await initBin();
  if (!devInit) {
    console.log("run dev servers");
    devViteProcesses = [
      await createServer({
        mode,
        configFile: r("./src/apps/main/vite.config.ts"),
        root: r("./src/apps/main"),
      }),
      await createServer({
        mode,
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
      }),
    ];
    buildViteProcesses = [
      await buildVite({
        mode,
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs/vite.config.ts"),
      }),
    ];
    devExecaProcesses.push(
      execa({
        preferLocal: true,
        stdout: "inherit",
        cwd: r("./src/apps/settings"),
      })`pnpm dev`,
    );
    await execa({
      preferLocal: true,
      stdout: "inherit",
      cwd: r("./src/apps/modules"),
    })`pnpm build`;

    if (mode === "test") {
      devExecaProcesses.push(
        execa({
          preferLocal: true,
          cwd: r("./src/apps/test"),
        })`node --import @swc-node/register/esm-register server.ts`,
      );
    }
    devInit = true;
  }
  devViteProcesses.forEach((p) => {
    p.listen();
  });
  await Promise.all([
    buildVite({
      mode,
      root: r("./src/apps/startup"),
      configFile: r("./src/apps/startup/vite.config.ts"),
    }),

    injectManifest("_dist/bin", true),
    (async () => {
      await injectXHTML("_dist/bin");
      await injectXHTMLDev("_dist/bin");
    })(),
    applyMixin("_dist/bin"),
    // (async () => {
    //   try {
    //     await fs.access("_dist/profile");
    //     await fs.rm("_dist/profile", { recursive: true });
    //   } catch {}
    // })(),
  ]);

  //await injectUserJS(`noraneko${VERSION}`);

  // try {
  //   await fs.access("./profile/test");

  //   try {
  //     await fs.access("_dist/profile/test");
  //     await fs.rm("_dist/profile/test");
  //   } catch {}
  //   // https://searchfox.org/mozilla-central/rev/b4a96f411074560c4f9479765835fa81938d341c/toolkit/xre/nsAppRunner.cpp#1514
  //   // 可能性はある、まだ必要はない
  // } catch {}

  let browser: Browser | undefined = undefined;
  // //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  // await fs.mkdir("./_dist/profile/test", { recursive: true });
  await savePrefsForProfile("./_dist/profile/test");
  await runBrowser();

  browser = await puppeteer.connect({
    browserWSEndpoint: "ws://127.0.0.1:5180/session",
    protocol: "webDriverBiDi",
  });

  //   headless: false,
  //   protocol: "webDriverBiDi",
  //   dumpio: true,
  //   browser: "firefox",
  //   executablePath: binPathExe,
  //   userDataDir: "./_dist/profile/test",
  //   args: ["-jsdebugger", "-url", "about:home"],
  //   extraPrefsFirefox: {
  //     "browser.newtabpage.enabled": true,
  //     //https://searchfox.org/mozilla-central/rev/02a4a649ed75ebaf3fbdf301c3d3137baf6842a1/devtools/shared/security/auth.js#170
  //     "devtools.debugger.prompt-connection": false,

  //     //? Thank you for `arai` san in Mozilla!
  //     //? This pref allows to run import of http(s) protocol in browser-top or about: pages
  //     //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/dom/security/nsContentSecurityManager.cpp#1037-1041
  //     //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/modules/libpref/init/StaticPrefList.yaml#15063
  //     "security.disallow_privileged_https_script_loads": false,
  //     //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/dom/security/nsContentSecurityUtils.cpp#1600-1607
  //     //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/dom/security/nsContentSecurityUtils.cpp#1445-1450
  //     //https://searchfox.org/mozilla-central/rev/6936c4c3fc9bee166912fce10104fbe0417d77d3/modules/libpref/init/StaticPrefList.yaml#14743
  //     "security.allow_parent_unrestricted_js_loads": true,

  //     //https://searchfox.org/mozilla-central/rev/71aada9d4055e420f91f3d0fa107f0328763e40b/browser/app/profile/firefox.js#1249
  //     "browser.preferences.moreFromMozilla": false,

  //     //https://searchfox.org/mozilla-central/rev/e968519d806b140c402c3b3932cd5f6cd7cc42ac/testing/profiles/unittest-required/user.js#111
  //     "extensions.screenshots.disabled": false,

  //     //https://searchfox.org/mozilla-central/rev/cc01f11adfacca9cd44a75fd140d2fdd8f9a48d4/browser/app/profile/firefox.js#307
  //     "browser.startup.page": 3,

  //     //* puppeteer seems to set homepage as about:blank
  //     //https://searchfox.org/mozilla-central/rev/aee7c3a0dbf33af0c4f6648f391db62b35895e50/browser/components/preferences/tests/browser_homepage_default.js#28
  //     "browser.startup.homepage": "about:home",

  //     "general.useragent.override":
  //       "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:130.0) Gecko/20100101 Firefox/130.0",

  //     "noraneko.features.enabled": `{"common":["browser-share-mode","context-menu","designs","downloadbar","overrides","private-container","profile-manager","reverse-sidebar-position","statusbar","tabbar","undo-closed-tab"]}`,
  //   },
  //   defaultViewport: { height: 0, width: 0 },
  //   timeout: 0,
  // });

  //await (await browser.pages())[0].goto("https://google.com");

  if (mode === "dev" && false) {
    // const pages = await browser.pages();
    // await pages[0].goto("about:newtab", {
    //   timeout: 0,
    //   waitUntil: "domcontentloaded",
    // });
    // //? We should not go to about:preferences
    // //? Puppeteer cannot get the load event so when reload, this errors.
    // // await pages[0].goto("about:preferences", {
    // //   timeout: 0,
    // //   waitUntil: "domcontentloaded",
    // // });
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

  // browser.on("disconnected", () => {
  //   process.exit();
  // });
}

/**
 * * Please run with NODE_ENV='production'
 * @param mode
 */
async function release(mode: "before" | "after") {
  if (mode === "before") {
    await Promise.all([
      buildVite({
        configFile: r("./src/apps/startup/vite.config.ts"),
        root: r("./src/apps/startup"),
      }),
      buildVite({
        configFile: r("./src/apps/main/vite.config.ts"),
        root: r("./src/apps/main"),
      }),
      buildVite({
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
      }),

      //applyMixin(binPath),
    ]);
    await injectManifest("./_dist", false);
  } else if (mode === "after") {
    const binPath = "../obj-x86_64-pc-windows-msvc/dist/bin";
    injectXHTML(binPath);
  }
}

if (process.argv[2]) {
  switch (process.argv[2]) {
    case "--run":
      run();
      break;
    case "--test":
      run("test");
      break;
    case "--release-build-before":
      release("before");
      break;
    case "--release-build-after":
      release("after");
      break;
  }
}
