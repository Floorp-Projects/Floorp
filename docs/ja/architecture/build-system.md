# ビルドシステム - 詳細ガイド

## ビルドシステム概要

Floorp のビルドシステムは、Firefox バイナリをベースにしたカスタムブラウザを構築するための複雑なパイプラインです。Deno をメインランタイムとして使用し、複数の言語やツールチェーンを統合しています。

## 全体アーキテクチャ

```
┌─────────────────────────────────────────────────────────────┐
│                  Floorpビルドシステム                       │
├─────────────────────────────────────────────────────────────┤
│  エントリーポイント: build.ts (Deno)                       │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │   バイナリ  │ │   パッチ    │ │   コードインジェクション│   │
│  │ 管理        │ │   適用      │ │   システム           │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ 開発サーバー│ │ 本番ビルド  │ │   配布パッケージング │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## メインビルドスクリプト (build.ts)

### 基本構造

```typescript
// エントリーポイント
import * as fs from "node:fs/promises";
import * as pathe from "pathe";
import { injectManifest } from "./scripts/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "./scripts/inject/xhtml.ts";
import { applyMixin } from "./scripts/inject/mixin-loader.ts";

// ブランディング設定
export const brandingBaseName = "floorp";
export const brandingName = "Floorp";

// プラットフォーム別設定
const VERSION = process.platform === "win32" ? "001" : "000";
const binExtractDir = "_dist/bin";
const binDir =
  process.platform !== "darwin"
    ? `_dist/bin/${brandingBaseName}`
    : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;
```

### 主な関数

#### 1. バイナリ管理

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

#### 2. プラットフォーム別バイナリアーカイブ取得

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

## ビルドパイプライン

### 1. 開発モード (`deno task dev`)

```
開発者コード変更
    ↓
ファイル監視システムが変更を検知
    ↓
TypeScript/SolidJSコンパイル (Vite)
    ↓
コードインジェクションシステム実行
    ↓
Firefoxバイナリへ変更を注入
    ↓
ブラウザのホットリロード
```

#### 開発サーバー起動

```typescript
async function run(mode: "dev" | "test" | "release" = "dev") {
  await initBin();
  await applyPatches();

  // バージョン情報生成
  await genVersion();

  if (mode !== "release") {
    if (!devInit) {
      // 子プロセスでビルドを実行
      await $`deno run -A ./scripts/launchDev/child-build.ts ${mode} ${
        buildid2 ?? ""
      }`;

      // 開発サーバー起動
      devViteProcess = $`deno run -A ./scripts/launchDev/child-dev.ts ${mode} ${
        buildid2 ?? ""
      }`;

      // ホットリロード対応
      await waitForDevServer();
      devInit = true;
    }

    // マニフェスト注入
    await Promise.all([
      injectManifest(binDir, "dev", "noraneko-dev"),
      injectXHTMLDev(binDir),
    ]);
  }

  // XHTML注入とミキシン適用
  await Promise.all([injectXHTML(binDir), applyMixin(binDir)]);

  // ブラウザ起動
  browserProcess = $`deno run -A ./scripts/launchDev/child-browser.ts`;
}
```

### 2. 本番ビルド (`deno task build`)

```
本番ビルド開始
    ↓
全アプリケーションの最適化ビルド
    ↓
アセット最適化＆圧縮
    ↓
コードインジェクション（本番設定）
    ↓
配布パッケージ生成
```

#### 本番ビルド処理

```typescript
async function release(mode: "before" | "after") {
  let buildid2: string | null = null;
  try {
    buildid2 = await fs.readFile("_dist/buildid2", { encoding: "utf-8" });
  } catch {
    console.warn("buildid2 not found");
  }

  if (mode === "before") {
    // 本番ビルド実行
    await $`deno run -A ./scripts/launchDev/child-build.ts production ${
      buildid2 ?? ""
    }`;
    await injectManifest("./_dist", "prod");
  } else if (mode === "after") {
    // 出力ディレクトリ検出と処理
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

    // 最終注入処理
    injectXHTML(binPath);
    await writeBuildid2(`${binPath}/browser`, buildid2 ?? "");
  }
}
```

## 子プロセス管理

### 1. child-build.ts - ビルド処理

```typescript
// 各アプリケーションの並列ビルド
const buildApps = async (mode: string) => {
  const apps = ["main", "settings", "newtab", "notes", "welcome"];

  await Promise.all(apps.map((app) => buildApp(app, mode)));
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

### 2. child-dev.ts - 開発サーバー

```typescript
// 複数の開発サーバーを並列起動
const startDevServers = async () => {
  const servers = [
    { name: "main", port: 3000 },
    { name: "settings", port: 3001 },
    { name: "newtab", port: 3002 },
  ];

  await Promise.all(
    servers.map((server) => startDevServer(server.name, server.port))
  );
};

const startDevServer = async (appName: string, port: number) => {
  const appDir = `src/apps/${appName}`;
  await $`cd ${appDir} && vite dev --port ${port}`;
};
```

### 3. child-browser.ts - ブラウザ起動

```typescript
// カスタマイズされたFirefoxを起動
const launchBrowser = async () => {
  const firefoxPath = getBrowserPath();
  const profilePath = "_dist/profile/test";

  const args = [
    "--profile",
    profilePath,
    "--no-remote",
    "--new-instance",
    ...(isDevelopment ? ["--jsconsole"] : []),
  ];

  await $`${firefoxPath} ${args}`;
};
```

## コードインジェクションシステム

### 1. マニフェスト注入 (manifest.ts)

```typescript
export async function injectManifest(
  binDir: string,
  mode: "dev" | "prod" | "run-prod",
  devDirName?: string
) {
  const manifestPath = path.join(binDir, "chrome.manifest");
  const customManifest = generateCustomManifest(mode, devDirName);

  // 既存のマニフェストに追加
  await fs.appendFile(manifestPath, customManifest);
}

const generateCustomManifest = (mode: string, devDirName?: string) => {
  const entries = [
    "content noraneko-content chrome://noraneko/content/",
    "resource noraneko resource://noraneko/",
    "locale noraneko en-US chrome://noraneko/locale/en-US/",
  ];

  return entries.map((entry) => `${entry}\n`).join("");
};
```

### 2. XHTML 注入 (xhtml.ts)

```typescript
export async function injectXHTML(binDir: string) {
  const browserXHTMLPath = path.join(
    binDir,
    "chrome/browser/content/browser/browser.xhtml"
  );

  // 既存のXHTMLファイルを読み込む
  const originalContent = await fs.readFile(browserXHTMLPath, "utf-8");

  // カスタムスクリプトを注入
  const injectedContent = injectCustomScripts(originalContent);

  // ファイルを更新
  await fs.writeFile(browserXHTMLPath, injectedContent);
}

const injectCustomScripts = (content: string): string => {
  const scriptTags = [
    '<script src="chrome://noraneko/content/main.js"></script>',
    '<script src="chrome://noraneko/content/startup.js"></script>',
  ];

  // </head>タグの前に注入
  return content.replace("</head>", `${scriptTags.join("\n")}\n</head>`);
};
```

### 3. ミキシンシステム (mixin-loader.ts)

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

## アセット処理

### 1. CSS 処理

```typescript
const processCSSFiles = async () => {
  const cssFiles = await glob("src/**/*.css");

  for (const cssFile of cssFiles) {
    // PostCSSで処理
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

### 2. 画像最適化

```typescript
const optimizeImages = async () => {
  const imageFiles = await glob("src/**/*.{png,jpg,jpeg,svg}");

  for (const imageFile of imageFiles) {
    const outputPath = imageFile.replace("src/", "_dist/");

    if (imageFile.endsWith(".svg")) {
      // SVG最適化
      await optimizeSVG(imageFile, outputPath);
    } else {
      // ビットマップ画像最適化
      await optimizeBitmap(imageFile, outputPath);
    }
  }
};
```

## パフォーマンス最適化

### 1. 並列処理

```typescript
// 複数のタスクを並列実行
const parallelBuild = async () => {
  await Promise.all([buildTypeScriptApps(), processAssets(), applyPatches()]);
};
```

### 2. インクリメンタルビルド

```typescript
const incrementalBuild = async () => {
  const changedFiles = await getChangedFiles();

  // 変更されたファイルのみを処理
  const affectedApps = getAffectedApps(changedFiles);
  await Promise.all(affectedApps.map(buildApp));
};
```

### 3. キャッシュシステム

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

## エラー処理と回復

### 1. 優雅なシャットダウン

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

### 2. ビルドエラー回復

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

      // 一時ファイルをクリーンアップ
      await cleanupTempFiles();

      // 指数バックオフで再試行
      await sleep(Math.pow(2, i) * 1000);
    }
  }
};
```

このビルドシステムにより、Floorp は複雑なマルチ言語、マルチプラットフォーム環境で効率的な開発と展開を実現します。
