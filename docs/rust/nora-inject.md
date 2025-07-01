# Nora-Inject - Rust ベースのコード注入システム

## 概要

Nora-Inject (`crates/nora-inject/`) は Floorp の高性能なコード注入システムです。Rust で実装され、WebAssembly として実行されることで、安全で高速なコード変換と注入を実現します。

## アーキテクチャ

### システム構成

```
┌─────────────────────────────────────────────────────────────┐
│                    Floorp Build System                      │
├─────────────────────────────────────────────────────────────┤
│  JavaScript/TypeScript Layer                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │   Build     │ │   Source    │ │    File System      │   │
│  │  Scripts    │ │   Files     │ │     Operations      │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  WebAssembly Interface (WIT)                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │              Component Interface                        │ │
│  └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  Nora-Inject (Rust/WASM)                                   │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Code Parser │ │ Transformer │ │   Code Generator    │   │
│  │    (OXC)    │ │   Engine    │ │      System         │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 主要コンポーネント

### 1. Cargo.toml - 依存関係設定

```toml
[package]
name = "nora-inject"
version = "0.1.0"
edition = "2021"

[lib]
name = "nora_inject_lib"
crate-type = ["cdylib"]

[dependencies]
oxc = { version = "0.31.0", features = ["serialize", "wasm"] }
serde = "1.0.210"
serde_json = "1.0.128"
wit-bindgen = "0.34.0"
```

### 2. lib.rs - メインライブラリ

```rust
// Nora-Inject メインライブラリ
use oxc::allocator::Allocator;
use oxc::parser::{Parser, ParserOptions};
use oxc::transformer::{TransformOptions, Transformer};
use oxc::codegen::{CodeGenerator, CodegenOptions};
use serde::{Deserialize, Serialize};
use wit_bindgen::generate;

// WebAssembly Component Interface Types
wit_bindgen::generate!({
    world: "nora-inject",
    path: "wit/world.wit"
});

// エクスポートする構造体
struct NoraInject;

// WIT インターフェースの実装
impl Guest for NoraInject {
    fn transform_code(source: String, options: TransformOptions) -> Result<String, String> {
        transform_javascript_code(&source, options)
            .map_err(|e| format!("Transform error: {}", e))
    }
    
    fn parse_ast(source: String) -> Result<String, String> {
        parse_to_ast(&source)
            .map_err(|e| format!("Parse error: {}", e))
    }
    
    fn inject_code(target: String, injection: String, position: InjectionPosition) -> Result<String, String> {
        inject_code_at_position(&target, &injection, position)
            .map_err(|e| format!("Injection error: {}", e))
    }
}

// 注入位置の定義
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum InjectionPosition {
    Top,
    Bottom,
    BeforeFunction(String),
    AfterFunction(String),
    ReplaceFunction(String),
    Custom(u32), // 行番号
}

// 変換オプション
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TransformOptions {
    pub target: String,           // ES5, ES2015, ES2020, etc.
    pub module_type: String,      // CommonJS, ESModule, UMD
    pub minify: bool,
    pub source_map: bool,
    pub preserve_comments: bool,
    pub custom_transforms: Vec<String>,
}

// メイン変換関数
pub fn transform_javascript_code(
    source: &str, 
    options: TransformOptions
) -> Result<String, Box<dyn std::error::Error>> {
    let allocator = Allocator::default();
    
    // パース
    let parser_options = ParserOptions {
        allow_return_outside_function: true,
        ..Default::default()
    };
    
    let mut parser = Parser::new(&allocator, source, parser_options);
    let program = parser.parse()?;
    
    // 変換
    let transform_options = create_transform_options(&options);
    let mut transformer = Transformer::new(&allocator, transform_options);
    transformer.build(&program)?;
    
    // コード生成
    let codegen_options = CodegenOptions {
        minify: options.minify,
        ..Default::default()
    };
    
    let mut codegen = CodeGenerator::new(codegen_options);
    codegen.build(&program);
    
    Ok(codegen.into_source_text())
}

// AST パース関数
pub fn parse_to_ast(source: &str) -> Result<String, Box<dyn std::error::Error>> {
    let allocator = Allocator::default();
    let parser_options = ParserOptions::default();
    let mut parser = Parser::new(&allocator, source, parser_options);
    let program = parser.parse()?;
    
    // AST をJSON形式でシリアライズ
    let ast_json = serde_json::to_string_pretty(&program)?;
    Ok(ast_json)
}

// コード注入関数
pub fn inject_code_at_position(
    target: &str,
    injection: &str,
    position: InjectionPosition
) -> Result<String, Box<dyn std::error::Error>> {
    match position {
        InjectionPosition::Top => {
            Ok(format!("{}\n{}", injection, target))
        },
        InjectionPosition::Bottom => {
            Ok(format!("{}\n{}", target, injection))
        },
        InjectionPosition::BeforeFunction(func_name) => {
            inject_before_function(target, injection, &func_name)
        },
        InjectionPosition::AfterFunction(func_name) => {
            inject_after_function(target, injection, &func_name)
        },
        InjectionPosition::ReplaceFunction(func_name) => {
            replace_function(target, injection, &func_name)
        },
        InjectionPosition::Custom(line_number) => {
            inject_at_line(target, injection, line_number as usize)
        },
    }
}

// 関数の前に注入
fn inject_before_function(
    source: &str,
    injection: &str,
    function_name: &str
) -> Result<String, Box<dyn std::error::Error>> {
    let allocator = Allocator::default();
    let parser_options = ParserOptions::default();
    let mut parser = Parser::new(&allocator, source, parser_options);
    let program = parser.parse()?;
    
    // AST を走査して関数を見つける
    let mut injector = FunctionInjector::new(injection, function_name, InjectionMode::Before);
    injector.visit_program(&program);
    
    if let Some(position) = injector.injection_position {
        inject_at_position(source, injection, position)
    } else {
        Err(format!("Function '{}' not found", function_name).into())
    }
}

// 関数の後に注入
fn inject_after_function(
    source: &str,
    injection: &str,
    function_name: &str
) -> Result<String, Box<dyn std::error::Error>> {
    let allocator = Allocator::default();
    let parser_options = ParserOptions::default();
    let mut parser = Parser::new(&allocator, source, parser_options);
    let program = parser.parse()?;
    
    let mut injector = FunctionInjector::new(injection, function_name, InjectionMode::After);
    injector.visit_program(&program);
    
    if let Some(position) = injector.injection_position {
        inject_at_position(source, injection, position)
    } else {
        Err(format!("Function '{}' not found", function_name).into())
    }
}

// 関数を置換
fn replace_function(
    source: &str,
    replacement: &str,
    function_name: &str
) -> Result<String, Box<dyn std::error::Error>> {
    let allocator = Allocator::default();
    let parser_options = ParserOptions::default();
    let mut parser = Parser::new(&allocator, source, parser_options);
    let program = parser.parse()?;
    
    let mut replacer = FunctionReplacer::new(replacement, function_name);
    replacer.visit_program(&program);
    
    if let Some((start, end)) = replacer.replacement_range {
        let mut result = String::new();
        result.push_str(&source[..start]);
        result.push_str(replacement);
        result.push_str(&source[end..]);
        Ok(result)
    } else {
        Err(format!("Function '{}' not found", function_name).into())
    }
}

// 指定行に注入
fn inject_at_line(
    source: &str,
    injection: &str,
    line_number: usize
) -> Result<String, Box<dyn std::error::Error>> {
    let lines: Vec<&str> = source.lines().collect();
    
    if line_number > lines.len() {
        return Err("Line number out of range".into());
    }
    
    let mut result = Vec::new();
    
    for (i, line) in lines.iter().enumerate() {
        if i == line_number - 1 {
            result.push(injection);
        }
        result.push(line);
    }
    
    Ok(result.join("\n"))
}

// 位置指定注入
fn inject_at_position(
    source: &str,
    injection: &str,
    position: usize
) -> Result<String, Box<dyn std::error::Error>> {
    if position > source.len() {
        return Err("Position out of range".into());
    }
    
    let mut result = String::new();
    result.push_str(&source[..position]);
    result.push_str(injection);
    result.push_str(&source[position..]);
    
    Ok(result)
}

// AST ビジター for 関数注入
struct FunctionInjector {
    injection: String,
    target_function: String,
    mode: InjectionMode,
    injection_position: Option<usize>,
}

#[derive(Debug, Clone)]
enum InjectionMode {
    Before,
    After,
}

impl FunctionInjector {
    fn new(injection: &str, target_function: &str, mode: InjectionMode) -> Self {
        Self {
            injection: injection.to_string(),
            target_function: target_function.to_string(),
            mode,
            injection_position: None,
        }
    }
    
    fn visit_program(&mut self, program: &oxc::ast::Program) {
        // AST を走査して関数を探す実装
        // 実際の実装では oxc の visitor trait を使用
    }
}

// AST ビジター for 関数置換
struct FunctionReplacer {
    replacement: String,
    target_function: String,
    replacement_range: Option<(usize, usize)>,
}

impl FunctionReplacer {
    fn new(replacement: &str, target_function: &str) -> Self {
        Self {
            replacement: replacement.to_string(),
            target_function: target_function.to_string(),
            replacement_range: None,
        }
    }
    
    fn visit_program(&mut self, program: &oxc::ast::Program) {
        // AST を走査して関数を探す実装
    }
}

// 変換オプションの作成
fn create_transform_options(options: &TransformOptions) -> TransformOptions {
    // OXC の TransformOptions に変換
    TransformOptions {
        // 実際の実装では適切なマッピングを行う
        ..Default::default()
    }
}

// WebAssembly エクスポート
export!(NoraInject);
```

### 3. WIT インターフェース定義

```wit
// wit/world.wit - WebAssembly Component Interface Types
package nora:inject@0.1.0;

world nora-inject {
  // コード変換インターフェース
  export transform-code: func(
    source: string,
    options: transform-options
  ) -> result<string, string>;
  
  // AST パースインターフェース
  export parse-ast: func(
    source: string
  ) -> result<string, string>;
  
  // コード注入インターフェース
  export inject-code: func(
    target: string,
    injection: string,
    position: injection-position
  ) -> result<string, string>;
}

// 変換オプション
record transform-options {
  target: string,
  module-type: string,
  minify: bool,
  source-map: bool,
  preserve-comments: bool,
  custom-transforms: list<string>,
}

// 注入位置
variant injection-position {
  top,
  bottom,
  before-function(string),
  after-function(string),
  replace-function(string),
  custom(u32),
}
```

## TypeScript/JavaScript 統合

### 1. WASM ローダー

```typescript
// scripts/inject/wasm/loader.ts
import { readFile } from "node:fs/promises";
import { join } from "node:path";

// WASM モジュールの型定義
export interface NoraInjectModule {
  transformCode(source: string, options: TransformOptions): string;
  parseAst(source: string): string;
  injectCode(target: string, injection: string, position: InjectionPosition): string;
}

export interface TransformOptions {
  target: string;
  moduleType: string;
  minify: boolean;
  sourceMap: boolean;
  preserveComments: boolean;
  customTransforms: string[];
}

export type InjectionPosition = 
  | { type: "top" }
  | { type: "bottom" }
  | { type: "beforeFunction"; functionName: string }
  | { type: "afterFunction"; functionName: string }
  | { type: "replaceFunction"; functionName: string }
  | { type: "custom"; lineNumber: number };

// WASM モジュールの読み込み
export async function loadNoraInject(): Promise<NoraInjectModule> {
  const wasmPath = join(import.meta.dirname, "nora-inject.wasm");
  const wasmBuffer = await readFile(wasmPath);
  
  const wasmModule = await WebAssembly.compile(wasmBuffer);
  const wasmInstance = await WebAssembly.instantiate(wasmModule, {
    // インポート関数の定義（必要に応じて）
  });
  
  const exports = wasmInstance.exports as any;
  
  return {
    transformCode: (source: string, options: TransformOptions): string => {
      try {
        const result = exports.transform_code(source, JSON.stringify(options));
        return JSON.parse(result);
      } catch (error) {
        throw new Error(`Transform failed: ${error}`);
      }
    },
    
    parseAst: (source: string): string => {
      try {
        const result = exports.parse_ast(source);
        return JSON.parse(result);
      } catch (error) {
        throw new Error(`Parse failed: ${error}`);
      }
    },
    
    injectCode: (target: string, injection: string, position: InjectionPosition): string => {
      try {
        const result = exports.inject_code(target, injection, JSON.stringify(position));
        return JSON.parse(result);
      } catch (error) {
        throw new Error(`Injection failed: ${error}`);
      }
    }
  };
}
```

### 2. 高レベル API

```typescript
// scripts/inject/code-injector.ts
import { loadNoraInject, type NoraInjectModule, type TransformOptions, type InjectionPosition } from "./wasm/loader.ts";

export class CodeInjector {
  private wasmModule: NoraInjectModule | null = null;
  
  async initialize(): Promise<void> {
    this.wasmModule = await loadNoraInject();
  }
  
  // JavaScript/TypeScript コードの変換
  async transformCode(source: string, options: Partial<TransformOptions> = {}): Promise<string> {
    if (!this.wasmModule) {
      throw new Error("WASM module not initialized");
    }
    
    const defaultOptions: TransformOptions = {
      target: "ES2020",
      moduleType: "ESModule",
      minify: false,
      sourceMap: false,
      preserveComments: true,
      customTransforms: [],
      ...options
    };
    
    return this.wasmModule.transformCode(source, defaultOptions);
  }
  
  // Firefox の JavaScript ファイルに関数を注入
  async injectIntoFirefoxScript(
    scriptPath: string,
    injectionCode: string,
    targetFunction?: string
  ): Promise<string> {
    if (!this.wasmModule) {
      throw new Error("WASM module not initialized");
    }
    
    const originalCode = await Deno.readTextFile(scriptPath);
    
    const position: InjectionPosition = targetFunction
      ? { type: "beforeFunction", functionName: targetFunction }
      : { type: "top" };
    
    return this.wasmModule.injectCode(originalCode, injectionCode, position);
  }
  
  // XUL ファイルにスクリプトタグを注入
  async injectScriptIntoXUL(
    xulPath: string,
    scriptSrc: string,
    position: "head" | "body" = "head"
  ): Promise<string> {
    if (!this.wasmModule) {
      throw new Error("WASM module not initialized");
    }
    
    const originalXUL = await Deno.readTextFile(xulPath);
    const scriptTag = `<script src="${scriptSrc}"></script>`;
    
    const injectionPosition: InjectionPosition = position === "head"
      ? { type: "beforeFunction", functionName: "</head>" }
      : { type: "beforeFunction", functionName: "</body>" };
    
    return this.wasmModule.injectCode(originalXUL, scriptTag, injectionPosition);
  }
  
  // CSS ファイルの注入
  async injectCSS(
    targetFile: string,
    cssCode: string,
    position: "top" | "bottom" = "bottom"
  ): Promise<string> {
    if (!this.wasmModule) {
      throw new Error("WASM module not initialized");
    }
    
    const originalCSS = await Deno.readTextFile(targetFile);
    
    const injectionPosition: InjectionPosition = {
      type: position
    };
    
    return this.wasmModule.injectCode(originalCSS, cssCode, injectionPosition);
  }
  
  // 複数ファイルの一括処理
  async batchInject(operations: InjectOperation[]): Promise<Map<string, string>> {
    const results = new Map<string, string>();
    
    for (const operation of operations) {
      try {
        let result: string;
        
        switch (operation.type) {
          case "script":
            result = await this.injectIntoFirefoxScript(
              operation.targetFile,
              operation.code,
              operation.targetFunction
            );
            break;
          case "xul":
            result = await this.injectScriptIntoXUL(
              operation.targetFile,
              operation.code,
              operation.position as "head" | "body"
            );
            break;
          case "css":
            result = await this.injectCSS(
              operation.targetFile,
              operation.code,
              operation.position as "top" | "bottom"
            );
            break;
          default:
            throw new Error(`Unknown operation type: ${operation.type}`);
        }
        
        results.set(operation.targetFile, result);
      } catch (error) {
        console.error(`Failed to process ${operation.targetFile}:`, error);
        throw error;
      }
    }
    
    return results;
  }
}

// 注入操作の定義
export interface InjectOperation {
  type: "script" | "xul" | "css";
  targetFile: string;
  code: string;
  targetFunction?: string;
  position?: string;
}

// グローバルインスタンス
export const codeInjector = new CodeInjector();
```

## 使用例

### 1. ビルドスクリプトでの使用

```typescript
// scripts/inject/inject-floorp-features.ts
import { codeInjector, type InjectOperation } from "./code-injector.ts";

async function injectFloorpFeatures(binDir: string): Promise<void> {
  await codeInjector.initialize();
  
  const operations: InjectOperation[] = [
    // ブラウザ起動時の初期化コード
    {
      type: "script",
      targetFile: `${binDir}/chrome/browser/content/browser/browser.js`,
      code: `
        // Floorp 初期化
        if (typeof FloorpBrowser === 'undefined') {
          var FloorpBrowser = {
            init: function() {
              console.log('Floorp Browser initialized');
              this.loadCustomModules();
            },
            loadCustomModules: function() {
              // カスタムモジュールの読み込み
            }
          };
          
          // ページ読み込み完了時に初期化
          window.addEventListener('load', () => FloorpBrowser.init());
        }
      `,
      targetFunction: "BrowserGlue"
    },
    
    // XUL にカスタムメニューを追加
    {
      type: "xul",
      targetFile: `${binDir}/chrome/browser/content/browser/browser.xhtml`,
      code: `
        <menu id="floorp-menu" label="Floorp">
          <menupopup>
            <menuitem label="設定" oncommand="FloorpBrowser.openSettings()"/>
            <menuitem label="about" oncommand="FloorpBrowser.openAbout()"/>
          </menupopup>
        </menu>
      `,
      position: "head"
    },
    
    // カスタム CSS の注入
    {
      type: "css",
      targetFile: `${binDir}/chrome/browser/skin/browser.css`,
      code: `
        /* Floorp カスタムスタイル */
        #floorp-menu {
          color: #0078d4;
          font-weight: bold;
        }
        
        .floorp-button {
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          border: none;
          border-radius: 6px;
          color: white;
          padding: 8px 16px;
        }
      `,
      position: "bottom"
    }
  ];
  
  const results = await codeInjector.batchInject(operations);
  
  // 結果をファイルに書き込み
  for (const [filePath, content] of results) {
    await Deno.writeTextFile(filePath, content);
    console.log(`✓ Injected code into ${filePath}`);
  }
}

export { injectFloorpFeatures };
```

### 2. 開発時の動的注入

```typescript
// scripts/inject/dev-inject.ts
import { codeInjector } from "./code-injector.ts";
import { watch } from "node:fs";

export async function setupDevInjection(sourceDir: string, targetDir: string): Promise<void> {
  await codeInjector.initialize();
  
  // ソースファイルの変更を監視
  watch(sourceDir, { recursive: true }, async (eventType, filename) => {
    if (eventType === 'change' && filename) {
      const sourcePath = `${sourceDir}/${filename}`;
      const targetPath = `${targetDir}/${filename}`;
      
      try {
        if (filename.endsWith('.js') || filename.endsWith('.ts')) {
          // JavaScript/TypeScript ファイルの変換と注入
          const sourceCode = await Deno.readTextFile(sourcePath);
          const transformedCode = await codeInjector.transformCode(sourceCode, {
            target: "ES2020",
            minify: false,
            sourceMap: true
          });
          
          await Deno.writeTextFile(targetPath, transformedCode);
          console.log(`🔄 Hot reloaded: ${filename}`);
        }
      } catch (error) {
        console.error(`❌ Failed to process ${filename}:`, error);
      }
    }
  });
  
  console.log(`👀 Watching ${sourceDir} for changes...`);
}
```

Nora-Inject により、Floorp は高性能で安全なコード注入システムを実現し、Firefox の既存コードベースを効率的にカスタマイズできます。