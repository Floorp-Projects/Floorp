import { usePwsh } from "npm:zx@^8.5.4/core";
import { buildForProduction, run, runWithInitBinGit } from "./scripts/build.ts";
import { genVersion } from "./scripts/launchDev/writeVersion.ts";

// Platform specific configurations
switch (Deno.build.os) {
  case "windows":
    usePwsh();
    break;
}

// Helper function to check if a file or directory exists
const isExists = async (path: string): Promise<boolean> => {
  try {
    await Deno.stat(path);
    return true;
  } catch {
    return false;
  }
};

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
      buildForProduction("before-mozbuild");
      break;
    case "--release-build-after":
      buildForProduction("after-mozbuild");
      break;
    case "--write-version":
      await genVersion();
      break;
  }
}
