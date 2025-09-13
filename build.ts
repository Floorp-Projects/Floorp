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
import { $, type ProcessPromise } from "zx";
import { usePwsh } from "zx";
import chalk from "chalk";
import process from "node:process";

switch (process.platform) {
  case "win32":
    usePwsh();
}

//? branding
export const brandingBaseName = "floorp";
export const brandingName = "Floorp";

//? when the linux binary has published, I'll sync linux bin version
const VERSION = process.platform === "win32" ? "001" : "000";
const binExtractDir = "_dist/bin";
const binDir = process.platform !== "darwin"
  ? `_dist/bin/${brandingBaseName}`
  : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;

const _r = (dir: string) => {
  return pathe.resolve(import.meta.dirname as string, dir);
};

const isExists = async (path: string) => {
  return await fs
    .access(path)
    .then(() => true)
    .catch(() => false);
};

const getBinArchive = () => {
  const arch = process.arch;
  if (process.platform === "win32") {
    return `${brandingBaseName}-win-amd64-moz-artifact.zip`;
  } else if (process.platform === "linux") {
    if (arch === "x64") {
      return `${brandingBaseName}-linux-amd64-moz-artifact.tar.xz`;
    } else if (arch === "arm64") {
      return `${brandingBaseName}-linux-aarch64-moz-artifact.tar.xz`;
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
} catch {
  // ignore
}

const binPath = pathe.join(binDir, brandingBaseName);
const binPathExe = process.platform !== "darwin"
  ? binPath + (process.platform === "win32" ? ".exe" : "")
  : `./_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/MacOS/${brandingBaseName}`;

const binVersion = pathe.join(binDir, "nora.version.txt");

async function decompressBin() {
  try {
    console.log(chalk.cyan(`📦 Decompressing ${binArchive}...`));
    if (!(await isExists(binArchive))) {
      console.log(
        chalk.yellow(
          `⚠️  ${binArchive} not found. We will download ${await getBinArchive()} from GitHub latest release.`,
        ),
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
      const macConfig = {
        mountDir: "_dist/mount",
        appDirName: "Contents",
        resourcesDirName: "Resources",
      };
      const mountDir = macConfig.mountDir;
      await fs.mkdir(mountDir, { recursive: true });
      await $`hdiutil ${["attach", "-mountpoint", mountDir, binArchive]}`;

      try {
        const appDestBase = pathe.join(
          "./_dist/bin",
          brandingBaseName,
          `${brandingName}.app`,
        );
        const destContents = pathe.join(appDestBase, macConfig.appDirName);
        await fs.mkdir(destContents, { recursive: true });
        await fs.mkdir(binDir, { recursive: true });

        const srcContents = pathe.join(
          mountDir,
          `${brandingName}.app/${macConfig.appDirName}`,
        );
        const items = await fs.readdir(srcContents);
        for (const item of items) {
          const srcItem = pathe.join(srcContents, item);
          if (item === macConfig.resourcesDirName) {
            const resourcesItems = await fs.readdir(srcItem);
            await Promise.all(
              resourcesItems.map((resItem) =>
                fs.cp(
                  pathe.join(srcItem, resItem),
                  pathe.join(binDir, resItem),
                  { recursive: true },
                )
              ),
            );
          } else {
            await fs.cp(srcItem, pathe.join(destContents, item), {
              recursive: true,
            });
          }
        }

        await fs.writeFile(pathe.join(binDir, "nora.version.txt"), VERSION);
        await Promise.all([
          $`chmod ${["-R", "777", appDestBase]}`,
          $`xattr ${["-rc", appDestBase]}`,
        ]);
      } finally {
        await $`hdiutil ${["detach", mountDir]}`;
        await fs.rm(mountDir, { recursive: true });
      }
    }

    if (process.platform === "linux") {
      //? linux
      await $`tar -xf ${binArchive} -C ${binExtractDir}`;
      await $`chmod ${["-R", "755", `./${binDir}`]}`;
      await $`chmod ${["755", binPathExe]}`;
    }

    console.log(chalk.green("✅ Decompress complete!"));
  } catch (e) {
    console.error(e);
    process.exit(1);
  }
}

async function downloadBinArchive() {
  const fileName = await getBinArchive();
  let originUrl = (await $`git remote get-url origin`).stdout.trim();
  if (originUrl.endsWith("/")) {
    originUrl = originUrl.slice(0, -1);
  }
  const originDownloadUrl =
    `${originUrl}-runtime/releases/latest/download/${fileName}`;
  console.log(chalk.blue(`⬇️  Downloading from origin: ${originDownloadUrl}`));
  try {
    await $`curl -L --fail --progress-bar -o ${binArchive} ${originDownloadUrl}`;
    console.log(chalk.green("✅ Download complete from origin!"));
  } catch (error: unknown) {
    console.error(
      chalk.red("❌ Origin download failed, falling back to upstream:"),
      (error as { stderr: string }).stderr,
    );
    const upstreamUrl =
      `https://github.com/Floorp-Projects/Floorp-runtime/releases/latest/download/${fileName}`;
    console.log(chalk.blue(`⬇️  Downloading from upstream: ${upstreamUrl}`));
    try {
      await $`curl -L --fail --progress-bar -o ${binArchive} ${upstreamUrl}`;
      console.log(chalk.green("✅ Download complete from upstream!"));
    } catch (error2: unknown) {
      console.error(
        chalk.red("❌ Upstream download failed:"),
        (error2 as { stderr: string }).stderr,
      );
      throw (error2 as { stderr: string }).stderr;
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
      console.log(
        chalk.yellow(`⚠️  Version mismatch ${version} !== ${VERSION}`),
      );
      await fs.rm(binDir, { recursive: true });
      await fs.mkdir(binDir, { recursive: true });
      await decompressBin();
      return;
    }
  } else {
    if (hasBin) {
      console.log(
        chalk.cyan(
          `📝 Bin exists, but version file not found, writing ${VERSION}`,
        ),
      );
      await fs.mkdir(binDir, { recursive: true });
      await fs.writeFile(binVersion, VERSION);
    }
  }
  console.log(chalk.magenta("🔧 Initializing binary..."));
  if (!hasBin) {
    console.log(chalk.yellow("⚠️  There seems no bin. decompressing."));
    await fs.mkdir(binDir, { recursive: true });
    await decompressBin();
  }
}

async function applyPrefs(targetBinDir?: string) {
  console.log(chalk.blue("⚙️  Applying Firefox preferences overrides..."));

  const actualBinDir = targetBinDir || binDir;
  const firefoxJsPath = pathe.join(
    actualBinDir,
    "browser",
    "defaults",
    "preferences",
    "firefox.js",
  );

  // Check if firefox.js exists
  if (!(await isExists(firefoxJsPath))) {
    console.warn(
      chalk.yellow(
        `⚠️  firefox.js not found at ${firefoxJsPath}, skipping preferences override`,
      ),
    );
    return;
  }

  // Determine which script to use based on platform
  const isWindows = process.platform === "win32";
  const scriptName = isWindows ? "override.ps1" : "override.sh";
  const overrideScriptPath = pathe.join(
    import.meta.dirname as string,
    "gecko",
    "pref",
    scriptName,
  );

  // Check if override script exists
  if (!(await isExists(overrideScriptPath))) {
    console.warn(
      chalk.yellow(
        `⚠️  ${scriptName} not found, skipping preferences override`,
      ),
    );
    return;
  }

  try {
    let result;

    if (isWindows) {
      // Run PowerShell script on Windows
      console.log(
        chalk.cyan(`🔧 Running PowerShell override script on ${firefoxJsPath}`),
      );
      result =
        await $`powershell.exe -ExecutionPolicy Bypass -File ${overrideScriptPath} ${firefoxJsPath}`;
    } else {
      // Run bash script on Unix-like systems
      await $`chmod +x ${overrideScriptPath}`;
      console.log(
        chalk.cyan(`🔧 Running bash override script on ${firefoxJsPath}`),
      );
      result = await $`${overrideScriptPath} ${firefoxJsPath}`;
    }

    // Display script output
    if (result.stdout) {
      console.log(chalk.gray("📄 Script output:"), result.stdout);
    }
    if (result.stderr) {
      console.log(chalk.gray("📄 Script stderr:"), result.stderr);
    }

    console.log(
      chalk.green("✅ Firefox preferences override completed successfully"),
    );
  } catch (error) {
    console.error(chalk.red("❌ Failed to apply preferences override:"), error);
    // Show the stderr output if available
    if (error && typeof error === "object") {
      if ("stdout" in error && error.stdout) {
        console.error(chalk.gray("📄 Script stdout:"), error.stdout);
      }
      if ("stderr" in error && error.stderr) {
        console.error(chalk.gray("📄 Script stderr:"), error.stderr);
      }
    }
    throw error;
  }
}

// Alias function for release mode
async function applyPrefsForPath(binPath: string) {
  await applyPrefs(binPath);
}

async function runWithInitBinGit() {
  if (await isExists(binDir)) {
    await fs.rm(binDir, { recursive: true, force: true });
  }

  await initBin();
  await initializeBinGit();
  await run();
}

async function clobber() {
  try {
    await fs.rm("_dist", { recursive: true });
  } catch {
    console.error(chalk.red("❌ Failed to remove _dist directory"));
  }
}

let devViteProcess: ProcessPromise | null = null;
let browserProcess: ProcessPromise | null = null;
let devInit = false;
const sapphillonProcess: ProcessPromise | null = null;

async function run(mode: "dev" | "test" | "release" = "dev") {
  await initBin();
  await applyPatches();
  await applyPrefs();

  //create version for dev
  await genVersion();
  let buildid2: string | null = null;
  try {
    await fs.access("_dist/buildid2");
    buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
  } catch {
    console.warn(chalk.yellow("⚠️  buildid2 not found"));
  }
  console.log(chalk.gray(`🔢 Build ID: ${buildid2}`));

  // env
  if (process.platform === "darwin") {
    process.env.MOZ_DISABLE_CONTENT_SANDBOX = "1";
  }

  if (mode !== "release") {
    if (!devInit) {
      await $`deno run -A ./scripts/launchDev/child-build.ts ${mode} ${
        buildid2 ?? ""
      }`;
      console.log(chalk.green("🚀 Starting dev servers..."));
      devViteProcess = $`deno run -A ./scripts/launchDev/child-dev.ts ${mode} ${
        buildid2 ?? ""
      }`
        .stdio("pipe")
        .nothrow();

      let resolve: ((value: void | PromiseLike<void>) => void) | undefined =
        undefined;
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
          process.stdout.write(temp);
        }
      })();
      (async () => {
        for await (const temp of devViteProcess.stderr) {
          process.stdout.write(temp);
        }
      })();
      (async () => {
        if (!sapphillonProcess) return;
        for await (const temp of sapphillonProcess.stdout) {
          process.stdout.write(temp);
        }
      })();
      (async () => {
        if (!sapphillonProcess) return;
        for await (const temp of sapphillonProcess.stderr) {
          process.stdout.write(temp);
        }
      })();
      await temp_prm;
      devInit = true;
    }
    await Promise.all([
      injectManifest(binDir, "dev", "noraneko-dev"),
      injectXHTMLDev(binDir),
    ]);
  } else {
    await release("before");
    await injectManifest(binDir, "run-prod", "noraneko-dev");
    try {
      await Deno.remove(`_dist/bin/${brandingBaseName}/noraneko-dev`, {
        recursive: true,
      });
    } catch {
      // ignore
    }
    await Deno.symlink(
      pathe.resolve(import.meta.dirname as string, "_dist/noraneko"),
      `_dist/bin/${brandingBaseName}/noraneko-dev`,
      { type: "junction" },
    );
  }

  await Promise.all([
    (async () => {
      await injectXHTML(binDir, mode === "dev");
    })(),
    applyMixin(binDir),
  ]);

  //https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await fs.mkdir("./_dist/profile/test", { recursive: true });
  await savePrefsForProfile("./_dist/profile/test");

  browserProcess = $`deno run -A ./scripts/launchDev/child-browser.ts`
    .stdio("pipe")
    .nothrow();

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
    } catch {
      console.error(chalk.red("❌ browserProcess error"));
    }
    exit();
  })();
}

let runningExit = false;
async function exit() {
  if (runningExit) return;
  runningExit = true;
  if (browserProcess) {
    console.log(chalk.blue("🛑 Start Shutdown browserProcess"));
    browserProcess.stdin.write("q");
    try {
      await browserProcess;
    } catch (e) {
      console.error(e);
    }
    console.log(chalk.blue("✅ End Shutdown browserProcess"));
  }
  if (devViteProcess) {
    console.log(chalk.blue("🛑 Start Shutdown devViteProcess"));
    devViteProcess.stdin.write("q");
    try {
      await devViteProcess;
    } catch (e) {
      console.error(e);
    }
    console.log(chalk.blue("✅ End Shutdown devViteProcess"));
  }
  if (sapphillonProcess) {
    console.log(chalk.blue("🛑 Start Shutdown sapphillonProcess"));
    try {
      sapphillonProcess.kill();
      await sapphillonProcess;
    } catch (e) {
      console.error(e);
    }
    console.log(chalk.blue("✅ End Shutdown sapphillonProcess"));
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
  } catch {
    console.warn(chalk.yellow("⚠️  buildid2 not found"));
  }
  console.log(chalk.gray(`🔢 Build ID: ${buildid2}`));
  if (mode === "before") {
    console.log(
      chalk.cyan(
        `🔧 Running: deno run -A ./scripts/launchDev/child-build.ts production ${
          buildid2 ?? ""
        }`,
      ),
    );
    await $`deno run -A ./scripts/launchDev/child-build.ts production ${
      buildid2 ?? ""
    }`;
    await injectManifest("./_dist", "prod");
  } else if (mode === "after") {
    let binPath: string;
    const baseDir = "../obj-artifact-build-output/dist";
    try {
      const files = await fs.readdir(baseDir);
      const appFiles = files.filter((file) => file.endsWith(".app"));
      if (appFiles.length > 0) {
        const appFile = appFiles[0];
        const appPath = `${baseDir}/${appFile}`;
        binPath = `${appPath}/Contents/Resources`;
        console.log(
          chalk.cyan(
            `📁 Using app bundle directory: ${appPath}/Contents/Resources`,
          ),
        );
      } else {
        binPath = `${baseDir}/bin`;
        console.log(chalk.cyan(`📁 Using bin directory: ${baseDir}/bin`));
      }
    } catch (error) {
      console.warn(chalk.yellow("⚠️  Error reading output directory:"), error);
      binPath = `${baseDir}/bin`;
    }

    // Apply Firefox preferences override
    try {
      await applyPrefsForPath(binPath);
    } catch (error: unknown) {
      console.warn(
        chalk.yellow(
          "⚠️  Failed to apply preferences override in release mode:",
        ),
        error,
      );
    }

    injectXHTML(binPath);
    let buildid2: string | null = null;
    try {
      await fs.access("_dist/buildid2");
      buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
    } catch {
      console.warn(chalk.yellow("⚠️  buildid2 not found"));
    }
    await writeBuildid2(`${binPath}/browser`, buildid2 ?? "");
  }
}

if (Deno.args[0]) {
  switch (Deno.args[0]) {
    case "--run":
      run();
      break;
    case "--clobber":
      clobber();
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
