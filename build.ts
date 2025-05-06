import chalk from "chalk";
import * as pathe from "pathe";
import { $, type ProcessPromise } from "zx";
import { usePwsh } from "zx";

import {
  applyPatches,
  initializeBinGit,
} from "./scripts/git-patches/git-patches-manager.ts";
import { applyMixin } from "./scripts/inject/mixin-loader.ts";
import { injectManifest } from "./scripts/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.ts";
import { savePrefsForProfile } from "./scripts/launchDev/savePrefs.ts";
import { genVersion } from "./scripts/launchDev/writeVersion.ts";
import { writeBuildid2 } from "./scripts/update/buildid2.ts";
import {
  binDir,
  binExtractDir,
  binPath,
  binPathExe,
  binVersion,
  brandingBaseName,
  brandingName,
  getBinArchive,
  VERSION,
} from "./scripts/defines.ts";
import { TextEncoder } from "node:util";
import { symlinkDirectory } from "./scripts/inject/symlink-directory.ts";

// Platform specific configurations
switch (Deno.build.os) {
  case "windows":
    usePwsh();
    break;
}

// Helper function to resolve paths
const resolvePath = (dir: string) => {
  const baseDir = import.meta.dirname !== undefined
    ? import.meta.dirname
    : Deno.cwd();
  return pathe.resolve(baseDir, dir);
};

// Helper function to check if a file or directory exists
const isExists = async (path: string): Promise<boolean> => {
  try {
    await Deno.stat(path);
    return true;
  } catch {
    return false;
  }
};

// Binary archive filename
const binArchive = getBinArchive();

/**
 * Decompresses the binary archive based on the platform.
 */
async function decompressBin() {
  try {
    console.log(`[dev] decompressing ${binArchive}`);

    if (!(await isExists(binArchive))) {
      console.log(
        `[dev] ${binArchive} not found. We will download ${await getBinArchive()} from GitHub latest release.`,
      );
      await downloadBinArchive();
      return;
    }

    switch (Deno.build.os) {
      case "windows":
        // Windows
        await $`Expand-Archive -LiteralPath ${binArchive} -DestinationPath ${binExtractDir}`;
        break;

      case "darwin":
        // macOS
        await decompressBinMacOS();
        break;

      case "linux":
        // Linux
        await $`tar -xf ${binArchive} -C ${binExtractDir}`;
        await $`chmod ${["-R", "755", `./${binDir}`]}`;
        await $`chmod ${["755", binPathExe]}`;
        break;
    }

    await Deno.writeTextFile(binVersion, VERSION);

    console.log("[dev] decompress complete!");
  } catch (error: any) {
    console.error("[dev] Error during decompression:", error);
    Deno.exit(1);
  }
}

/**
 * Decompresses the binary archive on macOS.
 */
async function decompressBinMacOS() {
  const macConfig = {
    mountDir: "_dist/mount",
    appDirName: "Contents",
    resourcesDirName: "Resources",
  };
  const mountDir = macConfig.mountDir;

  try {
    await Deno.mkdir(mountDir, { recursive: true });
    await $`hdiutil ${["attach", "-mountpoint", mountDir, binArchive]}`;

    try {
      const appDestBase = pathe.join(
        "./_dist/bin",
        brandingBaseName,
        `${brandingName}.app`,
      );
      const destContents = pathe.join(appDestBase, macConfig.appDirName);
      await Deno.mkdir(destContents, { recursive: true });
      await Deno.mkdir(binDir, { recursive: true });

      const srcContents = pathe.join(
        mountDir,
        `${brandingName}.app/${macConfig.appDirName}`,
      );
      const items: Deno.DirEntry[] = [];
      for await (const item of Deno.readDir(srcContents)) {
        items.push(item);
      }

      for (const item of items) {
        const srcItem = pathe.join(srcContents, item.name);
        if (item.name === macConfig.resourcesDirName) {
          const resourcesItems: Deno.DirEntry[] = [];
          for await (const resItem of Deno.readDir(srcItem)) {
            resourcesItems.push(resItem);
          }
          await Promise.all(
            resourcesItems.map((resItem) =>
              Deno.copyFile(
                pathe.join(srcItem, resItem.name),
                pathe.join(binDir, resItem.name),
              )
            ),
          );
        } else {
          await Deno.copyFile(srcItem, pathe.join(destContents, item.name));
        }
      }

      await Promise.all([
        $`chmod ${["-R", "777", appDestBase]}`,
        $`xattr ${["-rc", appDestBase]}`,
      ]);
    } finally {
      await $`hdiutil ${["detach", mountDir]}`;
      await Deno.remove(mountDir, { recursive: true });
    }
  } catch (error) {
    console.error("[dev] Error during macOS decompression:", error);
    Deno.exit(1);
  }
}

/**
 * Downloads the binary archive from GitHub releases.
 */
async function downloadBinArchive() {
  const fileName = getBinArchive();
  let originUrl = (await $`git remote get-url origin`).stdout.trim();
  if (originUrl.endsWith("/")) {
    originUrl = originUrl.slice(0, -1);
  }
  const originDownloadUrl =
    `${originUrl}-runtime/releases/latest/download/${fileName}`;

  console.log(`[dev] Downloading from origin: ${originDownloadUrl}`);
  try {
    await $`curl -L --fail --progress-bar -o ${binArchive} ${originDownloadUrl}`;
    console.log("[dev] Download complete from origin!");
  } catch (error: any) {
    console.error(
      "[dev] Origin download failed, falling back to upstream:",
      error.stderr,
    );

    const upstreamUrl =
      `https://github.com/nyanrus/noraneko-runtime/releases/latest/download/${fileName}`;
    console.log(`[dev] Downloading from upstream: ${upstreamUrl}`);

    try {
      await $`curl -L --fail --progress-bar -o ${binArchive} ${upstreamUrl}`;
      console.log("[dev] Download complete from upstream!");
    } catch (error2: any) {
      console.error("[dev] Upstream download failed:", error2.stderr);
      throw error2.stderr;
    }
  }

  await decompressBin();
}

/**
 * Initializes the binary by checking for existing version and binary files.
 */
async function initBin() {
  const hasVersion = await isExists(binVersion);
  const hasBin = await isExists(binPathExe);

  if (hasVersion) {
    const version = await Deno.readTextFile(binVersion);
    const mismatch = VERSION !== version.trim();

    if (mismatch) {
      console.log(`[dev] version mismatch ${version.trim()} !== ${VERSION}`);
      console.log(
        "[dev] Removing existing binary directory and decompressing new binary.",
      );
      await Deno.remove(binDir, { recursive: true });
      await Deno.mkdir(binDir, { recursive: true });
      await decompressBin();
      return;
    }
    console.log("[dev] Binary version matches. Initialization complete.");
    return;
  }

  if (hasBin) {
    console.log(
      `[dev] Binary exists, but version file not found. Writing ${VERSION} to version file.`,
    );
    await Deno.mkdir(binDir, { recursive: true });
    await Deno.writeTextFile(binVersion, VERSION);
    console.log("[dev] Initialization complete.");
    return;
  }

  console.log("[dev] No binary found. Decompressing binary.");
  await Deno.mkdir(binDir, { recursive: true });
  await decompressBin();
  console.log("[dev] Initialization complete.");
}

/**
 * Initializes the binary and git, then runs the application.
 */
async function runWithInitBinGit() {
  if (await isExists(binDir)) {
    await Deno.remove(binDir, { recursive: true });
  }

  await initBin();
  await initializeBinGit();
  await run();
}

let devViteProcess: ProcessPromise | null = null;
let browserProcess: ProcessPromise | null = null;
let devInit = false;

/**
 * Runs the application in different modes (dev, test, release).
 * @param mode The mode to run the application in.
 */
async function run(mode: "dev" | "test" | "release" = "dev") {
  await initBin();
  await applyPatches();

  // Create version for dev
  await genVersion();

  let buildid2: string | null = null;
  try {
    await Deno.stat("_dist/buildid2");
    buildid2 = await Deno.readTextFile("_dist/buildid2");
  } catch {
    // Ignore if '_dist/buildid2' does not exist
  }
  console.log(`[dev] buildid2: ${buildid2}`);

  // Environment configuration for macOS
  if (Deno.build.os === "darwin") {
    Deno.env.set("MOZ_DISABLE_CONTENT_SANDBOX", "1");
  }

  if (mode !== "release") {
    await runDevMode(mode, buildid2);
  } else {
    await runReleaseMode();
  }

  await Promise.all([
    (async () => {
      await injectXHTML(binDir);
    })(),
    applyMixin(binDir),
    (async () => {
      try {
        await Deno.stat("_dist/profile");
        await Deno.remove("_dist/profile", { recursive: true });
      } catch {
        // Ignore if '_dist/profile' does not exist
      }
    })(),
  ]);

  // https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await Deno.mkdir("./_dist/profile/test", { recursive: true });
  await savePrefsForProfile("./_dist/profile/test");

  browserProcess = $`deno run -A ./scripts/launchDev/child-browser.ts`.stdio(
    "pipe",
  ).nothrow();

  (async () => {
    for await (const temp of browserProcess.stdout) {
      Deno.stdout.write(new TextEncoder().encode(temp));
    }
  })();
  (async () => {
    for await (const temp of browserProcess.stderr) {
      Deno.stdout.write(new TextEncoder().encode(temp));
    }
  })();
  (async () => {
    try {
      await browserProcess;
    } catch {
      // Ignore errors
    }
    exit();
  })();
}

/**
 * Runs the application in development mode.
 * @param mode The mode to run the application in.
 * @param buildid2 The build ID.
 */
async function runDevMode(mode: string, buildid2: string | null) {
  if (!devInit) {
    await symlinkDirectory(
      `apps/system/loader-features`,
      "apps/features-chrome",
      "link-features-chrome",
    );
    await symlinkDirectory(
      `apps/system/loader-features`,
      "i18n/features-chrome",
      "link-i18n-features-chrome",
    );
    await symlinkDirectory(
      `apps/system/loader-modules`,
      "apps/modules",
      "link-modules",
    );
    await $`deno run -A ./scripts/launchDev/child-build.ts ${mode} ${
      buildid2 ?? ""
    }`;
    console.log("[dev] run dev servers");
    devViteProcess = $`deno run -A ./scripts/launchDev/child-dev.ts ${mode} ${
      buildid2 ?? ""
    }`.stdio("pipe").nothrow();

    let resolve: (() => void) | undefined = undefined;
    const temp_prm = new Promise<void>((rs, _rj) => {
      resolve = rs;
    });
    (async () => {
      for await (const temp of devViteProcess.stdout) {
        if (
          temp.includes("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev")
        ) {
          if (resolve) (resolve as () => void)();
        }
        await Deno.stdout.write(new TextEncoder().encode(temp));
      }
    })();
    (async () => {
      for await (const temp of devViteProcess.stderr) {
        await Deno.stdout.write(new TextEncoder().encode(temp));
      }
    })();
    await temp_prm;
    devInit = true;
  }
  await Promise.all([
    injectManifest(binDir, "dev", "noraneko-dev"),
    injectXHTMLDev(binDir),
  ]);
}

/**
 * Runs the application in release mode.
 */
async function runReleaseMode() {
  await release("before");
  await injectManifest(binDir, "run-prod", "noraneko-dev");
  try {
    await Deno.remove(`_dist/bin/${brandingBaseName}/noraneko-dev`, {
      recursive: true,
    });
  } catch {
    // Ignore if the directory does not exist
  }
  await symlinkDirectory(
    `_dist/bin/${brandingBaseName}`,
    "_dist/noraneko",
    "noraneko-dev",
  );
}

let runningExit = false;

/**
 * Exits the application gracefully.
 */
async function exit() {
  if (runningExit) return;
  runningExit = true;

  if (browserProcess) {
    console.log("[dev] Start Shutdown browserProcess");
    browserProcess.stdin.write("q");
    try {
      await browserProcess;
    } catch (error) {
      console.error("[dev]", error);
    }
    console.log("[dev] End Shutdown browserProcess");
  }

  if (devViteProcess) {
    console.log("[dev] Start Shutdown devViteProcess");
    devViteProcess.stdin.write("q");
    try {
      await devViteProcess;
    } catch (error) {
      console.error("[dev]", error);
    }
    console.log("[dev] End Shutdown devViteProcess");
  }

  console.log(chalk.green("[dev] Cleanup Complete!"));
  Deno.exit(0);
}

/**
 * * Please run with NODE_ENV='production'
 * @param mode
 */
async function release(mode: "before" | "after") {
  let buildid2: string | null = null;
  try {
    await Deno.stat("_dist/buildid2");
    buildid2 = await Deno.readTextFile("_dist/buildid2");
  } catch {}
  console.log(`[dev] buildid2: ${buildid2}`);
  if (mode === "before") {
    console.log(
      `[dev] deno run -A ./scripts/launchDev/child-build.ts production ${
        buildid2 ?? ""
      }`,
    );
    await $`deno run -A ./scripts/launchDev/child-build.ts production ${
      buildid2 ?? ""
    }`;
    await injectManifest("./_dist", "prod");
  } else if (mode === "after") {
    let binPath: string;
    const baseDir = "../obj-artifact-build-output/dist";
    try {
      const files = await Deno.readDir(baseDir);
      const appFiles = [];
      for await (const file of files) {
        if (file.name.endsWith(".app")) {
          appFiles.push(file.name);
        }
      }
      if (appFiles.length > 0) {
        const appFile = appFiles[0];
        const appPath = `${baseDir}/${appFile}`;
        binPath = `${appPath}/Contents/Resources`;
        console.log(
          `[dev] Using app bundle directory: ${appPath}/Contents/Resources`,
        );
      } else {
        binPath = `${baseDir}/bin`;
        console.log(`[dev] Using bin directory: ${baseDir}/bin`);
      }
    } catch (error) {
      console.warn("[dev] Error reading output directory:", error);
      binPath = `${baseDir}/bin`;
    }
    injectXHTML(binPath);
    let buildid2: string | null = null;
    try {
      await Deno.stat("_dist/buildid2");
      buildid2 = await Deno.readTextFile("_dist/buildid2");
    } catch {}
    await writeBuildid2(`${binPath}/browser`, buildid2 ? buildid2 : "");
  }
}

if (Deno.args[0]) {
  switch (Deno.args[0]) {
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
