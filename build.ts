import * as fs from "node:fs/promises";
import * as path from "node:path";
import { injectManifest } from "./scripts/inject/manifest.js";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.js";
import { applyMixin } from "./scripts/inject/mixin-loader.js";
import puppeteer, { type Browser } from "puppeteer-core";
import { createServer, type ViteDevServer, build as buildVite } from "vite";
import AdmZip from "adm-zip";
import { execa, type ResultPromise } from "execa";
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

    new AdmZip(binArchive).extractAllTo(binDir);

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

let devViteProcesses: ViteDevServer[] | null = null;
let buildViteProcesses: any[];
const devExecaProcesses: ResultPromise[] = [];
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
  devViteProcesses!.forEach((p) => {
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
    (async () => {
      try {
        await fs.access("_dist/profile");
        await fs.rm("_dist/profile", { recursive: true });
      } catch {}
    })(),
  ]);

  let browser: Browser | undefined = undefined;
  // //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await fs.mkdir("./_dist/profile/test", { recursive: true });
  await savePrefsForProfile("./_dist/profile/test");
  await runBrowser();

  browser = await puppeteer.connect({
    browserWSEndpoint: "ws://127.0.0.1:5180/session",
    protocol: "webDriverBiDi",
  });

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

process.on("exit", () => {
  devExecaProcesses.forEach((v) => {
    v.kill();
  });
  devViteProcesses?.forEach(async (v) => {
    await v.close();
  });
});

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
