# Build System - Detailed Guide

## Build System Overview

Floorp's build system is a complex pipeline for building custom browsers based on Firefox binaries. It uses Deno as the main runtime and integrates multiple languages and toolchains.

## Overall Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  Floorp Build System                        │
├─────────────────────────────────────────────────────────────┤
│  Entry Point: build.ts (Deno)                              │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │   Binary    │ │    Patch    │ │   Code Injection    │   │
│  │ Management  │ │ Application │ │      System         │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ TypeScript  │ │    Rust     │ │   Asset Processing  │   │
│  │ Compilation │ │   WebASM    │ │   & Optimization    │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Development │ │ Production  │ │    Distribution     │   │
│  │   Server    │ │    Build    │ │     Packaging       │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Main Build Script (build.ts)

### Basic Structure
```typescript
// Entry point
import * as fs from "node:fs/promises";
import * as pathe from "pathe";
import { injectManifest } from "./scripts/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.ts";
import { applyMixin } from "./scripts/inject/mixin-loader.ts";

// Branding configuration
export const brandingBaseName = "floorp";
export const brandingName = "Floorp";

// Platform-specific configuration
const VERSION = process.platform === "win32" ? "001" : "000";
const binExtractDir = "_dist/bin";
const binDir = process.platform !== "darwin"
  ? `_dist/bin/${brandingBaseName}`
  : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;
```

### Main Functions

#### 1. Binary Management
```typescript
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
  }
  
  if (!hasBin) {
    console.log("There seems no bin. decompressing.");
    await fs.mkdir(binDir, { recursive: true });
    await decompressBin();
  }
}
```

#### 2. Platform-specific Binary Acquisition
```typescript
const getBinArchive = () => {
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
```

## Build Pipeline

### 1. Development Mode (`deno task dev`)

```
Developer Code Changes
    ↓
File Watcher System Detects Changes
    ↓
TypeScript/SolidJS Compilation (Vite)
    ↓
Rust Component Build (if needed)
    ↓
Code Injection System Execution
    ↓
Inject Changes into Firefox Binary
    ↓
Browser Hot Reload
```

#### Development Server Startup
```typescript
async function run(mode: "dev" | "test" | "release" = "dev") {
  await initBin();
  await applyPatches();
  
  // Generate version information
  await genVersion();
  
  if (mode !== "release") {
    if (!devInit) {
      // Execute build in child process
      await $`deno run -A ./scripts/launchDev/child-build.ts ${mode} ${buildid2 ?? ""}`;
      
      // Start development server
      devViteProcess = $`deno run -A ./scripts/launchDev/child-dev.ts ${mode} ${buildid2 ?? ""}`;
      
      // Support hot reload
      await waitForDevServer();
      devInit = true;
    }
    
    // Manifest injection
    await Promise.all([
      injectManifest(binDir, "dev", "noraneko-dev"),
      injectXHTMLDev(binDir),
    ]);
  }
  
  // XHTML injection and mixin application
  await Promise.all([
    injectXHTML(binDir),
    applyMixin(binDir),
  ]);
  
  // Launch browser
  browserProcess = $`deno run -A ./scripts/launchDev/child-browser.ts`;
}
```

### 2. Production Build (`deno task build`)

```
Start Production Build
    ↓
Optimized Build of All Applications
    ↓
Release Build of Rust Components
    ↓
Asset Optimization & Compression
    ↓
Code Injection (Production Configuration)
    ↓
Generate Distribution Package
```

#### Production Build Process
```typescript
async function release(mode: "before" | "after") {
  let buildid2: string | null = null;
  try {
    buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
  } catch {
    console.warn("buildid2 not found");
  }
  
  if (mode === "before") {
    // Execute production build
    await $`deno run -A ./scripts/launchDev/child-build.ts production ${buildid2 ?? ""}`;
    await injectManifest("./_dist", "prod");
  } else if (mode === "after") {
    // Detect and process output directory
    const baseDir = "../obj-artifact-build-output/dist";
    let binPath: string;
    
    try {
      const files = await fs.readdir(baseDir);
      const appFiles = files.filter((file) => file.endsWith(".app"));
      if (appFiles.length > 0) {
        binPath = `${baseDir}/${appFiles[0]}/Contents/Resources`;
      } else {
        binPath = `${baseDir}/bin`;
      }
    } catch (error) {
      binPath = `${baseDir}/bin`;
    }
    
    // Final injection processing
    injectXHTML(binPath);
    await writeBuildid2(`${binPath}/browser`, buildid2 ?? "");
  }
}
```

## Child Process Management

### 1. child-build.ts - Build Processing
```typescript
// Parallel build of each application
const buildApps = async (mode: string) => {
  const apps = ["main", "settings", "newtab", "notes", "welcome"];
  
  await Promise.all(apps.map(app => 
    buildApp(app, mode)
  ));
};

const buildApp = async (appName: string, mode: string) => {
  const appDir = `src/apps/${appName}`;
  const viteConfig = `${appDir}/vite.config.ts`;
  
  if (mode === "production") {
    await $`cd ${appDir} && vite build`;
  } else {
    await $`cd ${appDir} && vite build --mode development`;
  }
};
```

### 2. child-dev.ts - Development Server
```typescript
// Start multiple development servers in parallel
const startDevServers = async () => {
  const servers = [
    { name: "main", port: 3000 },
    { name: "settings", port: 3001 },
    { name: "newtab", port: 3002 },
  ];
  
  await Promise.all(servers.map(server => 
    startDevServer(server.name, server.port)
  ));
};

const startDevServer = async (appName: string, port: number) => {
  const appDir = `src/apps/${appName}`;
  await $`cd ${appDir} && vite dev --port ${port}`;
};
```

### 3. child-browser.ts - Browser Launch
```typescript
// Launch customized Firefox
const launchBrowser = async () => {
  const firefoxPath = getBrowserPath();
  const profilePath = "_dist/profile/test";
  
  const args = [
    "--profile", profilePath,
    "--no-remote",
    "--new-instance",
    ...(isDevelopment ? ["--jsconsole"] : [])
  ];
  
  await $`${firefoxPath} ${args}`;
};
```

## Code Injection System

### 1. Manifest Injection (manifest.ts)
```typescript
export async function injectManifest(
  binDir: string, 
  mode: "dev" | "prod" | "run-prod", 
  devDirName?: string
) {
  const manifestPath = path.join(binDir, "chrome.manifest");
  const customManifest = generateCustomManifest(mode, devDirName);
  
  // Append to existing manifest
  await fs.appendFile(manifestPath, customManifest);
}

const generateCustomManifest = (mode: string, devDirName?: string) => {
  const entries = [
    "content noraneko-content chrome://noraneko/content/",
    "resource noraneko resource://noraneko/",
    "locale noraneko en-US chrome://noraneko/locale/en-US/",
  ];
  
  return entries.map(entry => `${entry}\n`).join("");
};
```

### 2. XHTML Injection (xhtml.ts)
```typescript
export async function injectXHTML(binDir: string) {
  const browserXHTMLPath = path.join(binDir, "chrome/browser/content/browser/browser.xhtml");
  
  // Read existing XHTML file
  const originalContent = await fs.readFile(browserXHTMLPath, "utf-8");
  
  // Inject custom scripts
  const injectedContent = injectCustomScripts(originalContent);
  
  // Update file
  await fs.writeFile(browserXHTMLPath, injectedContent);
}

const injectCustomScripts = (content: string): string => {
  const scriptTags = [
    '<script src="chrome://noraneko/content/main.js"></script>',
    '<script src="chrome://noraneko/content/startup.js"></script>',
  ];
  
  // Inject before </head> tag
  return content.replace('</head>', `${scriptTags.join('\n')}\n</head>`);
};
```

### 3. Mixin System (mixin-loader.ts)
```typescript
export async function applyMixin(binDir: string) {
  const mixinDir = "scripts/inject/mixins";
  const mixinFiles = await fs.readdir(mixinDir, { recursive: true });
  
  for (const mixinFile of mixinFiles) {
    if (mixinFile.endsWith(".mixin.js")) {
      await applyMixinFile(path.join(mixinDir, mixinFile), binDir);
    }
  }
}

const applyMixinFile = async (mixinPath: string, binDir: string) => {
  const mixin = await import(mixinPath);
  const targetFile = path.join(binDir, mixin.targetPath);
  
  if (await fs.exists(targetFile)) {
    const content = await fs.readFile(targetFile, "utf-8");
    const modifiedContent = mixin.apply(content);
    await fs.writeFile(targetFile, modifiedContent);
  }
};
```

## Rust/WebAssembly Build

### 1. nora-inject Crate Build
```typescript
const buildRustComponents = async () => {
  // Build for WebAssembly target
  await $`cd crates/nora-inject && cargo build --target wasm32-wasi --release`;
  
  // Copy WebAssembly binary to appropriate location
  const wasmSource = "crates/nora-inject/target/wasm32-wasi/release/nora_inject_lib.wasm";
  const wasmDest = "scripts/inject/wasm/nora-inject.wasm";
  
  await fs.copyFile(wasmSource, wasmDest);
};
```

### 2. WebAssembly Integration
```typescript
// Load and execute WebAssembly module
const loadWasmModule = async () => {
  const wasmPath = "scripts/inject/wasm/nora-inject.wasm";
  const wasmBuffer = await fs.readFile(wasmPath);
  
  const wasmModule = await WebAssembly.compile(wasmBuffer);
  const wasmInstance = await WebAssembly.instantiate(wasmModule, {
    // Define import functions
  });
  
  return wasmInstance.exports;
};
```

## Asset Processing

### 1. CSS Processing
```typescript
const processCSSFiles = async () => {
  const cssFiles = await glob("src/**/*.css");
  
  for (const cssFile of cssFiles) {
    // Process with PostCSS
    const css = await fs.readFile(cssFile, "utf-8");
    const result = await postcss([
      autoprefixer,
      cssnano({ preset: "default" }),
    ]).process(css, { from: cssFile });
    
    const outputPath = cssFile.replace("src/", "_dist/");
    await fs.writeFile(outputPath, result.css);
  }
};
```

### 2. Image Optimization
```typescript
const optimizeImages = async () => {
  const imageFiles = await glob("src/**/*.{png,jpg,jpeg,svg}");
  
  for (const imageFile of imageFiles) {
    const outputPath = imageFile.replace("src/", "_dist/");
    
    if (imageFile.endsWith(".svg")) {
      // SVG optimization
      await optimizeSVG(imageFile, outputPath);
    } else {
      // Bitmap image optimization
      await optimizeBitmap(imageFile, outputPath);
    }
  }
};
```

## Performance Optimization

### 1. Parallel Processing
```typescript
// Execute multiple tasks in parallel
const parallelBuild = async () => {
  await Promise.all([
    buildTypeScriptApps(),
    buildRustComponents(),
    processAssets(),
    applyPatches(),
  ]);
};
```

### 2. Incremental Build
```typescript
const incrementalBuild = async () => {
  const changedFiles = await getChangedFiles();
  
  // Process only changed files
  const affectedApps = getAffectedApps(changedFiles);
  await Promise.all(affectedApps.map(buildApp));
};
```

### 3. Cache System
```typescript
const buildWithCache = async (target: string) => {
  const cacheKey = await generateCacheKey(target);
  const cachedResult = await getFromCache(cacheKey);
  
  if (cachedResult && !isStale(cachedResult)) {
    return cachedResult;
  }
  
  const result = await buildTarget(target);
  await saveToCache(cacheKey, result);
  
  return result;
};
```

## Error Handling and Recovery

### 1. Graceful Shutdown
```typescript
process.on("SIGINT", async () => {
  console.log("Shutting down gracefully...");
  
  if (browserProcess) {
    browserProcess.kill();
  }
  
  if (devViteProcess) {
    devViteProcess.kill();
  }
  
  await cleanup();
  process.exit(0);
});
```

### 2. Build Error Recovery
```typescript
const buildWithRetry = async (target: string, maxRetries: number = 3) => {
  for (let i = 0; i < maxRetries; i++) {
    try {
      return await buildTarget(target);
    } catch (error) {
      console.warn(`Build attempt ${i + 1} failed:`, error);
      
      if (i === maxRetries - 1) {
        throw error;
      }
      
      // Clean up temporary files
      await cleanupTempFiles();
      
      // Retry with exponential backoff
      await sleep(Math.pow(2, i) * 1000);
    }
  }
};
```

This build system enables Floorp to achieve efficient development and deployment in complex multi-language, multi-platform environments.