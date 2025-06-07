/**
 * Noraneko Build System - Root Entry Point
 *
 * This file provides backward compatibility and a simple entry point
 * that delegates to the actual build system in tools/build.ts.
 *
 * For detailed build system documentation, see docs/BUILD_SYSTEM_IMPROVEMENTS.md
 */

import { buildAndLaunch } from "./tools/build.ts";

if (Deno.args[0]) {
  switch (Deno.args[0]) {
    case "--dev": {
      await buildAndLaunch({ mode: "dev", launchBrowser: true });
      break;
    }
    case "--production": {
      await buildAndLaunch({ mode: "production" });
      break;
    }
    case "--dev-before": {
      await buildAndLaunch({ mode: "dev", phase: "before" });
      break;
    }
    case "--dev-after": {
      await buildAndLaunch({
        mode: "dev",
        phase: "after",
        launchBrowser: true,
      });
      break;
    }
    case "--production-before": {
      await buildAndLaunch({ mode: "production", phase: "before" });
      break;
    }
    case "--production-after": {
      await buildAndLaunch({ mode: "production", phase: "after" });
      break;
    }
    case "--release-build-before": {
      await buildAndLaunch({ mode: "production", phase: "before" });
      break;
    }
    case "--release-build-after": {
      await buildAndLaunch({ mode: "production", phase: "after" });
      break;
    }
    case "--write-version": {
      // Import genVersion only when needed
      const { genVersion } = await import(
        "./tools/dev/launchDev/writeVersion.ts"
      );
      await genVersion();
      break;
    }
    default: {
      console.log(`
🏗️  Noraneko Build System

Usage:
  deno run -A build.ts [option]

Development (recommended):
  --dev                       Full development build with browser launch
  (no args)                   Same as --dev

Production CI/CD Phases:
  --production-before         Production build - before phase
  --production-after          Production build - after phase  
  --release-build-before      CI/CD production build - before phase
  --release-build-after       CI/CD production build - after phase

Development Split Phases (if needed):
  --dev-before                Development build - before phase
  --dev-after                 Development build - after phase with browser launch

Utilities:
  --write-version             Write version info only

Typical Workflows:

Development:
  deno task dev               # Full build + launch browser

Production (CI/CD):
  1. deno task prod-before    # Prepare Noraneko scripts  
  2. ./mach build             # External Mozilla build
  3. deno task prod-after     # Apply injections

Examples:
  deno run -A build.ts
  deno run -A build.ts --dev
  deno task dev

For detailed documentation, see docs/BUILD_SYSTEM_IMPROVEMENTS.md
      `);
      break;
    }
  }
} else {
  // Default to development build with browser launch
  await buildAndLaunch({ mode: "dev", launchBrowser: true });
}
