import { task } from "hereby";
import { $ } from "zx";
import pathe from "pathe";
import { copy } from "@std/fs/copy";
import {
  binDir,
  binExtractDir,
  binPathExe,
  binVersion,
  brandingBaseName,
  brandingName,
  getBinArchive,
  VERSION,
} from "./scripts/defines.ts";
import { usePwsh } from "npm:zx@^8.5.3/core";
import { applyPatches } from "./scripts/git-patches/git-patches-manager.ts";
import { symlinkDirectory } from "./scripts/inject/symlink-directory.ts";

$.verbose = true;

if (Deno.build.os === "windows") usePwsh();

/**
 * Downloads the binary archive from either the origin or upstream URL.
 */
const downloadBinArchiveTask = task({
  name: "downloadBinArchive",
  run: async () => {
    const fileName = await getBinArchive();
    let originUrl = (await $`git remote get-url origin`).stdout.trim();
    // Remove trailing slash if present
    if (originUrl.endsWith("/")) {
      originUrl = originUrl.slice(0, -1);
    }

    const originDownloadUrl = `${originUrl}-runtime/releases/latest/download/${fileName}`;
    console.log(`[build] Downloading from origin: ${originDownloadUrl}`);

    try {
      const curlResult =
        await $`curl -L --fail --progress-bar -o ${binArchive} ${originDownloadUrl}`;
      if (curlResult.exitCode === 0) {
        console.log("[dev] Download complete from origin!");
        return;
      }
      console.error(
        "[dev] Origin download failed, falling back to upstream:",
        curlResult.stderr,
      );
    } catch (error) {
      console.error(`[dev] Origin download failed: ${error}`);
    }

    // Fallback to upstream URL
    const upstreamUrl =
      `https://github.com/nyanrus/noraneko-runtime/releases/latest/download/${fileName}`;
    console.log(`[dev] Downloading from upstream: ${upstreamUrl}`);

    try {
      const curlResult2 =
        await $`curl -L --fail --progress-bar -o ${binArchive} ${upstreamUrl}`;
      if (curlResult2.exitCode === 0) {
        console.log("[dev] Download complete from upstream!");
        return;
      }
      console.error("[dev] Upstream download failed:", curlResult2.stderr);
      throw curlResult2;
    } catch (error) {
      console.error(`[dev] Upstream download failed: ${error}`);
      throw error;
    }
  },
});

async symlinkBuildLoaders() {
  await symlinkDirectory(
    "./apps/system/loader-features",
    "./apps/features-chrome",
    "features-chrome",
  );
  await symlinkDirectory(
    "./apps/system/loader-features",
    "./i18n/features-chrome",
    "i18n-features-chrome",
  );
}
    console.log("[dev] Unzipping the binary for dev.");

    if (Deno.build.os === "windows") {
      // Windows
      await $`Microsoft.PowerShell.Archive\\Expand-Archive -Path ${binArchive} -DestinationPath ${binExtractDir}`;
      await Deno.writeTextFile(binVersion, VERSION);
    } else if (Deno.build.os === "linux") {
      // Linux
      await $`tar -xf ${binArchive} -C ${binExtractDir}`;
      await $`chmod -R 755 ${binDir}`;
      await $`chmod 755 ${binPathExe}`;
    } else if (Deno.build.os === "darwin") {
      // macOS
      const macConfig = {
        mountDir: "_dist/mount",
        appDirName: "Contents",
        resourcesDirName: "Resources",
      };
      const mountDir = macConfig.mountDir;
      await Deno.mkdir(mountDir, { recursive: true });
      await $`hdiutil attach -mountpoint ${mountDir} ${binArchive}`;

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
        const items = await Deno.readDir(srcContents);
        for await (const item of items) {
          const srcItem = pathe.join(srcContents, item.name);
          if (item.name === macConfig.resourcesDirName) {
            const resourcesItems = Deno.readDirSync(srcItem);
            await Promise.all(resourcesItems.map((resItem) =>
              copy(
                pathe.join(srcItem, resItem.name),
                pathe.join(binDir, resItem.name),
              )
            ));
          } else {
            await copy(srcItem, pathe.join(destContents, item.name));
          }
        }

        await Deno.writeTextFile(
          pathe.join(binDir, "nora.version.txt"),
          VERSION,
        );
        await Promise.all([
          $`chmod -R 777 ${appDestBase}`,
          $`xattr -rc ${appDestBase}`,
        ]);
      } finally {
        await $`hdiutil detach ${mountDir}`;
        await Deno.remove(mountDir, { recursive: true });
      }
    } else {
      console.warn(`[dev] Unknown OS: ${Deno.build.os}. Skipping binary decompression.`);
      return;
    }

    console.log("[dev] Unzipping the binary is complete!");
  } catch (error) {
    console.error(`[dev] Error decompressing binary: ${error}`);
    Deno.exit(1);
  }
}

/**
 * Decompresses the binary archive based on the operating system.
 */
async function decompressBin() {
  try {
    await Deno.mkdir(binDir, { recursive: true });
    if (!(await isExists(binArchive))) {
      console.log(
        `[dev] ${binArchive} not found. downloading ${getBinArchive()} from GitHub latest release.`,
      );
      await downloadBinArchiveTask();
    }
    console.log("[dev] Unzipping the binary for dev.");

    if (Deno.build.os === "windows") {
      // Windows
      await $`Microsoft.PowerShell.Archive\\Expand-Archive -Path ${binArchive} -DestinationPath ${binExtractDir}`;
      await Deno.writeTextFile(binVersion, VERSION);
    } else if (Deno.build.os === "linux") {
      // Linux
      await $`tar -xf ${binArchive} -C ${binExtractDir}`;
      await $`chmod -R 755 ${binDir}`;
      await $`chmod 755 ${binPathExe}`;
    } else if (Deno.build.os === "darwin") {
      // macOS
      const macConfig = {
        mountDir: "_dist/mount",
        appDirName: "Contents",
        resourcesDirName: "Resources",
      };
      const mountDir = macConfig.mountDir;
      await Deno.mkdir(mountDir, { recursive: true });
      await $`hdiutil attach -mountpoint ${mountDir} ${binArchive}`;

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
        const items = await Deno.readDir(srcContents);
        for await (const item of items) {
          const srcItem = pathe.join(srcContents, item.name);
          if (item.name === macConfig.resourcesDirName) {
            const resourcesItems = Deno.readDirSync(srcItem);
            await Promise.all(resourcesItems.map((resItem) =>
              copy(
                pathe.join(srcItem, resItem.name),
                pathe.join(binDir, resItem.name),
              )
            ));
          } else {
            await copy(srcItem, pathe.join(destContents, item.name));
          }
        }

        await Deno.writeTextFile(
          pathe.join(binDir, "nora.version.txt"),
          VERSION,
        );
        await Promise.all([
          $`chmod -R 777 ${appDestBase}`,
          $`xattr -rc ${appDestBase}`,
        ]);
      } finally {
        await $`hdiutil detach ${mountDir}`;
        await Deno.remove(mountDir, { recursive: true });
      }
    } else {
      console.warn(`[dev] Unknown OS: ${Deno.build.os}. Skipping binary decompression.`);
      return;
    }

    console.log("[dev] Unzipping the binary is complete!");
  } catch (error) {
    console.error(`[dev] Error decompressing binary: ${error}`);
    Deno.exit(1);
  }
}

async function symlinkBuildLoaders() {
  await symlinkDirectory(
    "./apps/system/loader-features",
    "./apps/features-chrome",
    "features-chrome",
  );
  await symlinkDirectory(
    "./apps/system/loader-features",
    "./i18n/features-chrome",
    "i18n-features-chrome",
  );
}

async function symlinkBuildLoaders() {
  await symlinkDirectory(
    "./apps/system/loader-features",
    "./apps/features-chrome",
    "features-chrome",
  );
  await symlinkDirectory(
    "./apps/system/loader-features",
    "./i18n/features-chrome",
    "i18n-features-chrome",
  );
}

/**
 * Reads the buildid2 file if it exists.
 * @returns {Promise<string | null>} The content of the buildid2 file, or null if it doesn't exist.
 */
async function readBuildid2() {
  try {
    // Asynchronously check existence and read the file
    const buildid2 = await Deno.readTextFile("_dist/buildid2");
    console.log(`[dev] buildid2: ${buildid2}`);
    return buildid2;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      // File not found, return null
      return null;
    }
    // Other error, re-throw
    throw error;
  }
}

const devChildBuild = task({
  "name": "devChildBuild",
  "run": async () => {
    const buildid2 = await readBuildid2();
    await $`deno run -A ./scripts/launchDev/child-build.ts dev ${
      buildid2 ?? ""
    }`;
  }
});

const devChildDev = task({
  name: "devChildDev",
  run: async () => {
    const buildid2 = await readBuildid2();
    const devViteProcess = $`deno run -A ./scripts/launchDev/child-dev.ts dev ${
      buildid2 ?? ""
    }`.stdio("pipe").nothrow();

    /** @type {Function | undefined} */
    let resolve = undefined;
    /** @type {Promise<void>} */
    const temp_prm = new Promise((rs, _rj) => {
      resolve = () => rs();
    });
    (async () => {
      for await (const temp of devViteProcess.stdout) {
        if (
          temp.includes("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev")
        ) {
          if (resolve) (/** @type {Function} */ (resolve))();
        } 
        Deno.stdout.write(temp);
      }
    })();
    (async () => {
      for await (const temp of devViteProcess.stderr) {
        Deno.stdout.write(temp);
      }
    })();
  }
})
