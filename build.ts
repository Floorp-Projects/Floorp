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
import { writeVersion } from "./scripts/update/version.js";
import { writeBuildid2 } from "./scripts/update/buildid2.js";
import { applyPatches } from "./scripts/git-patches/git-patches-manager.js";
import { initializeBinGit } from "./scripts/git-patches/git-patches-manager.js";

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

const getBinArchive = () => {
  if (process.platform === "win32") {
    return "noraneko-win-amd64-dev.zip";
  } if (process.platform === "linux") {
    const arch = process.arch;
    if (arch === "arm64") {
      return "noraneko-linux-aarch64-dev.zip";
    } if (arch === "x64") {
      return "noraneko-linux-amd64-dev.zip";
    }
  }
  throw new Error("Unsupported platform/architecture");
};

const binArchive = getBinArchive();
const binDir = "_dist/bin";

try {
  await fs.access("dist");
  await fs.rename("dist", "_dist");
} catch {}

const binPath = path.join(binDir, "noraneko");
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

    if (process.platform === "linux") {
      try {
        await execa("chmod", ["-R", "755", `./${binDir}`]);
        await execa("chmod", ["755", binPathExe]);
      } catch (chmodError) {
        process.exit(1);
      }
    }
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

async function runWithInitBinGit() {
  if (await isExists(binDir)) {
    await fs.rm(binDir, { recursive: true, force: true });
  }

  await initBin();
  await initializeBinGit();
  await run();
}

let devViteProcesses: ViteDevServer[] | null = null;
let buildViteProcesses: any[];
const devExecaProcesses: ResultPromise[] = [];
let devInit = false;

async function run(mode: "dev" | "test" = "dev") {
  await initBin();
  await applyPatches();
  let buildid2: string | null = null;
  try {
    await fs.access("_dist/buildid2");
    buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
  } catch {}
  console.log(`[dev] buildid2: ${buildid2}`);
  if (!devInit) {
    console.log("run dev servers");
    devViteProcesses = [
      await createServer({
        mode,
        configFile: r("./src/apps/main/vite.config.ts"),
        root: r("./src/apps/main"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
        },
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
    await execa({
      preferLocal: true,
      stdout: "inherit",
      cwd: r("./src/apps/modules"),
    })`pnpm genJarManifest`;

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

    injectManifest("_dist/bin", true, "noraneko-dev"),
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
  //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
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
  let buildid2: string | null = null;
  try {
    await fs.access("_dist/buildid2");
    buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
  } catch {}
  console.log(`[build] buildid2: ${buildid2}`);
  if (mode === "before") {
    await Promise.all([
      buildVite({
        configFile: r("./src/apps/startup/vite.config.ts"),
        root: r("./src/apps/startup"),
      }),
      buildVite({
        configFile: r("./src/apps/main/vite.config.ts"),
        root: r("./src/apps/main"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
        },
        base: "chrome://noraneko/content"
      }),
      buildVite({
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
      }),
      buildVite({
        configFile: r("./src/apps/settings/vite.config.ts"),
        root: r("./src/apps/settings"),
        base: "chrome://noraneko-settings/content"
      }),

      //applyMixin(binPath),
    ]);
    await execa({
      preferLocal: true,
      stdout: "inherit",
      cwd: r("./src/apps/modules"),
    })`pnpm build`;
    await execa({
      preferLocal: true,
      stdout: "inherit",
      cwd: r("./src/apps/modules"),
    })`pnpm genJarManifest`;
    await injectManifest("./_dist", false);
  } else if (mode === "after") {
    const binPath = "../obj-x86_64-pc-windows-msvc/dist/bin";
    injectXHTML(binPath);
    let buildid2: string | null = null;
    try {
      await fs.access("_dist/buildid2");
      buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
    } catch {}
    await writeBuildid2(`${binPath}/browser`, buildid2 ?? "");
    // await applyPatches(binPath);
  }
}

import { v7 as uuidv7 } from "uuid";
async function version() {
  await writeVersion(r("./gecko"));
  try {
    await fs.access("_dist");
  } catch {
    await fs.mkdir("_dist");
  }
  await writeBuildid2(r("./_dist"), uuidv7());
}

if (process.argv[2]) {
  switch (process.argv[2]) {
    case "--run":
      run();
      break;
    case "--run-with-init-bin-git":
      runWithInitBinGit();
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
    case "--write-version":
      version();
      break;
  }
}
