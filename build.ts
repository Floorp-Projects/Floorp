import { usePwsh } from "npm:zx@^8.5.4/core";
import { build } from "./tools/build/index.ts";
import { genVersion } from "./tools/dev/launchDev/writeVersion.ts";

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

// Main build entry point for Noraneko
// This script provides a simplified interface to the new modular build system

if (Deno.args[0]) {
  switch (Deno.args[0]) {
    case "--dev":
      await build({ mode: "dev" });
      break;
    case "--production":
      await build({ mode: "production" });
      break;
    case "--dev-skip-mozbuild":
      await build({ mode: "dev", skipMozbuild: true });
      break;
    case "--production-skip-mozbuild":
      await build({ mode: "production", skipMozbuild: true });
      break;
    case "--write-version":
      await genVersion();
      break;
    default:
      console.log(`
🏗️  Noraneko Build System

Usage:
  deno run -A build.ts [option]

Options:
  --dev                        Development build
  --production                 Production build
  --dev-skip-mozbuild         Development build (skip Mozilla build)
  --production-skip-mozbuild  Production build (skip Mozilla build)
  --write-version             Write version info only

Examples:
  deno run -A build.ts --dev
  deno run -A build.ts --production
      `);
      break;
  }
} else {
  // Default to development build
  await build({ mode: "dev" });
}
