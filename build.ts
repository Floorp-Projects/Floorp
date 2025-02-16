import * as fs from "node:fs/promises";
import * as pathe from "pathe";
import { injectManifest } from "./scripts/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.ts";
import { applyMixin } from "./scripts/inject/mixin-loader.ts";
import AdmZip from "adm-zip";
import { savePrefsForProfile } from "./scripts/launchDev/savePrefs.ts";

import { applyPatches } from "./scripts/git-patches/git-patches-manager.ts";
import { initializeBinGit } from "./scripts/git-patches/git-patches-manager.ts";
import { genVersion } from "./scripts/launchDev/writeVersion.ts";
import { writeBuildid2 } from "./scripts/update/buildid2.ts";
import { $, ProcessPromise } from "zx";
import { usePwsh } from "zx";
import chalk from "chalk";
import { expandGlob } from "@std/fs";
import process from "node:process";

switch (process.platform) {
  case "win32":
    usePwsh();
}

//? branding
const brandingBaseName = "noraneko";
const brandingName = "Noraneko";

//? when the linux binary has published, I'll sync linux bin version
const VERSION = process.platform === "win32" ? "001" : "000";
const binExtractDir = "_dist/bin";
const binDir = process.platform !== "darwin"
  ? `_dist/bin/${brandingBaseName}`
  : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;

const r = (dir: string) => {
  return pathe.resolve(import.meta.dirname, dir);
};

const isExists = async (path: string) => {
  return await fs
    .access(path)
    .then(() => true)
    .catch(() => false);
};

const getBinArchive = async () => {
  const arch = process.arch;
  if (process.platform === "win32") {
    return `${brandingBaseName}-win-amd64-moz-artifact.zip`;
  } else if (process.platform === "linux") {
    if (arch === "x64") {
      return `${brandingBaseName}-linux-amd64-moz-artifact.tar.xz`;
    } else if (arch === "arm64") {
      return `${brandingBaseName}-linux-arm64-moz-artifact.tar.xz`;
    }
  } else if (process.platform === "darwin") {
    return `${brandingBaseName}-macOS-universal-moz-artifact.dmg`;
  }
  throw new Error("Unsupported platform/architecture");
};

const binArchive = await getBinArchive();

try {
  await fs.access("dist");
  await fs.rename("dist", "_dist");
} catch {}

const binPath = pathe.join(binDir, brandingBaseName);
const binPathExe = process.platform !== "darwin"
  ? binPath + (process.platform === "win32" ? ".exe" : "")
  : `./_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/MacOS/${brandingBaseName}`;

const binVersion = pathe.join(binDir, "nora.version.txt");

async function decompressBin() {
  try {
    console.log(`decompressing ${binArchive}`);
    if (!(await isExists(binArchive))) {
      console.log(
        `${binArchive} not found. We will download ${await getBinArchive()} from GitHub latest release.`,
      );
      await downloadBinArchive();
      return;
    }

    if (process.platform === "win32") {
      //? windows
      new AdmZip(binArchive).extractAllTo(binExtractDir);
      await fs.writeFile(binVersion, VERSION);
    }

    if (process.platform === "darwin") {
      //? macOS
      const mountDir = "_dist/mount";
      await fs.mkdir(mountDir, { recursive: true });
      await $`hdiutil ${["attach", "-mountpoint", mountDir, binArchive]}`;
      await fs.mkdir(binDir, { recursive: true });
      await fs.cp(
        pathe.join(mountDir, `${brandingName}.app`),
        pathe.join(`./_dist/bin/${brandingBaseName}`, `${brandingName}.app`),
        { recursive: true },
      );
      await fs.writeFile(binVersion, VERSION);
      await $`hdiutil ${["detach", mountDir]}`;
      await fs.rm(mountDir, { recursive: true });
      await $`chmod ${[
        "-R",
        "777",
        `./_dist/bin/${brandingBaseName}/${brandingName}.app`,
      ]}`;
      await $`xattr ${[
        "-rc",
        `./_dist/bin/${brandingBaseName}/${brandingName}.app`,
      ]}`;
    }

    if (process.platform === "linux") {
      //? linux
      await $`tar -xf ${binArchive} -C ${binExtractDir}`;
      await $`chmod ${["-R", "755", `./${binDir}`]}`;
      await $`chmod ${["755", binPathExe]}`;
    }

    console.log("decompress complete!");
  } catch (e) {
    console.error(e);
    process.exit(1);
  }
}

async function downloadBinArchive() {
  const fileName = await getBinArchive();
  const originUrl = (await $`git remote get-url origin`).stdout.trim();
  const originDownloadUrl = `${originUrl}releases/latest/download/${fileName}`;
  console.log(`Downloading from origin: ${originDownloadUrl}`);
  try {
    await $`curl -L --fail --progress-bar -o ${binArchive} ${originDownloadUrl}`;
    console.log("Download complete from origin!");
  } catch (error) {
    console.error(
      "Origin download failed, falling back to upstream:",
      error.stderr,
    );
    const upstreamUrl =
      `https://github.com/nyanrus/noraneko/releases/latest/download/${fileName}`;
    console.log(`Downloading from upstream: ${upstreamUrl}`);
    try {
      await $`curl -L --fail --progress-bar -o ${binArchive} ${upstreamUrl}`;
      console.log("Download complete from upstream!");
    } catch (error2) {
      console.error("Upstream download failed:", error2.stderr);
      throw error2.stderr;
    }
  }
  await decompressBin();
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

let devViteProcess: ProcessPromise | null = null;
let browserProcess: ProcessPromise | null = null;
let devInit = false;

async function run(mode: "dev" | "test" | "release" = "dev") {
  await initBin();
  await applyPatches();

  //create version for dev
  await genVersion();
  let buildid2: string | null = null;
  try {
    await fs.access("_dist/buildid2");
    buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
  } catch {}
  console.log(`[dev] buildid2: ${buildid2}`);
  if (mode !== "release") {
    if (!devInit) {
      await $`deno run -A ./scripts/launchDev/child-build.ts ${mode} ${
        buildid2 ?? ""
      }`;
      console.log("run dev servers");
      devViteProcess = $`deno run -A ./scripts/launchDev/child-dev.ts ${mode} ${
        buildid2 ?? ""
      }`.stdio("pipe").nothrow();

      let resolve: Function | undefined = undefined;
      const temp_prm = new Promise<void>((rs, _rj) => {
        resolve = rs;
      });
      (async () => {
        for await (const temp of devViteProcess.stdout) {
          if (
            temp.includes("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev")
          ) {
            if (resolve) (resolve as Function)();
          }
          process.stdout.write(temp);
        }
      })();
      (async () => {
        for await (const temp of devViteProcess.stderr) {
          process.stdout.write(temp);
        }
      })();
      await temp_prm;

      // env
      if (process.platform === "darwin") {
        process.env.MOZ_DISABLE_CONTENT_SANDBOX = "1";
      }
      devInit = true;
    }
    await Promise.all([
      injectManifest(binDir, true, "noraneko-dev"),
      injectXHTMLDev(binDir),
    ]);
  } else {
    await release("before");
    try {
      await fs.access(`_dist/bin/${brandingBaseName}/noraneko-dev`);
      await fs.rm(`_dist/bin/${brandingBaseName}/noraneko-dev`, {
        recursive: true,
      });
    } catch {}
    await fs.symlink(
      `../../${brandingBaseName}`,
      `./_dist/bin/${brandingBaseName}/noraneko-dev`,
      process.platform === "win32" ? "junction" : undefined,
    );
  }

  await Promise.all([
    (async () => {
      await injectXHTML(binDir);
    })(),
    applyMixin(binDir),
    (async () => {
      try {
        await fs.access("_dist/profile");
        await fs.rm("_dist/profile", { recursive: true });
      } catch {}
    })(),
  ]);

  //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await fs.mkdir("./_dist/profile/test", { recursive: true });
  await savePrefsForProfile("./_dist/profile/test");

  browserProcess = $`deno run -A ./scripts/launchDev/child-browser.ts`.stdio(
    "pipe",
  ).nothrow();

  (async () => {
    for await (const temp of browserProcess.stdout) {
      process.stdout.write(temp);
    }
  })();
  (async () => {
    for await (const temp of browserProcess.stderr) {
      process.stdout.write(temp);
    }
  })();
  (async () => {
    try {
      await browserProcess;
    } catch {}
    exit();
  })();
}

let runningExit = false;
async function exit() {
  if (runningExit) return;
  runningExit = true;
  if (browserProcess) {
    console.log("[build] Start Shutdown browserProcess");
    browserProcess.stdin.write("q");
    try {
      await browserProcess;
    } catch (e) {
      console.error(e);
    }
    console.log("[build] End Shutdown browserProcess");
  }
  if (devViteProcess) {
    console.log("[build] Start Shutdown devViteProcess");
    devViteProcess.stdin.write("q");
    try {
      await devViteProcess;
    } catch (e) {
      console.error(e);
    }
    console.log("[build] End Shutdown devViteProcess");
  }
  console.log(chalk.green("[build] Cleanup Complete!"));
  process.exit(0);
}

process.on("SIGINT", async () => {
  await exit();
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
    await $`deno run -A ./scripts/launchDev/child-build.ts production ${
      buildid2 ?? ""
    }`;
    await injectManifest("./_dist", false);
  } else if (mode === "after") {
    const binPath = "../obj-artifact-build-output/dist/bin";
    injectXHTML(binPath);
    let buildid2: string | null = null;
    try {
      await fs.access("_dist/buildid2");
      buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
    } catch {}
    await writeBuildid2(`${binPath}/browser`, buildid2 ?? "");
  }
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
    case "--run-prod":
      run("release");
      break;
    case "--release-build-before":
      release("before");
      break;
    case "--release-build-after":
      release("after");
      break;
    case "--write-version":
      await genVersion();
      break;
  }
}
