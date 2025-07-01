# Nora-Inject - Rust Code Injection System

## Overview

Nora-Inject (`crates/nora-inject/`) is a high-performance code injection system written in Rust that enables Floorp to safely and efficiently inject custom code into Firefox binaries. This system uses WebAssembly for secure execution and provides a bridge between Rust's performance and TypeScript's development convenience.

## System Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                  Nora-Inject Architecture                   │
├─────────────────────────────────────────────────────────────┤
│  TypeScript/JavaScript Layer                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │   Build     │ │   Runtime   │ │   API Bindings      │   │
│  │  Scripts    │ │   Loader    │ │   & Wrappers        │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  WebAssembly Interface Layer                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │    WIT      │ │   WASM      │ │   Memory & Type     │   │
│  │ Definitions │ │  Runtime    │ │   Management        │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Rust Core Implementation                                  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │   Parser    │ │  Injector   │ │   File System       │   │
│  │   Engine    │ │   Engine    │ │   Operations        │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  System Integration Layer                                  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │   File I/O  │ │   Process   │ │   Security &        │   │
│  │  Operations │ │ Management  │ │   Validation        │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Crate Structure

```
crates/nora-inject/
├── src/                    # Rust source code
│   ├── lib.rs             # Library entry point
│   ├── inject.rs          # Core injection logic
│   ├── parser.rs          # File parsing and analysis
│   ├── filesystem.rs      # File system operations
│   ├── validation.rs      # Security validation
│   ├── error.rs           # Error handling
│   └── utils.rs           # Utility functions
├── wit/                   # WebAssembly Interface Types
│   └── world.wit          # Interface definitions
├── tests/                 # Test files
│   ├── integration.rs     # Integration tests
│   ├── unit.rs           # Unit tests
│   └── fixtures/         # Test fixtures
├── benches/               # Benchmark tests
│   └── performance.rs     # Performance benchmarks
├── Cargo.toml             # Rust package configuration
├── package.json           # npm package configuration
├── README.md              # Package documentation
└── *.wasm                 # Compiled WebAssembly binaries
```

## Cargo.toml Configuration

```toml
[package]
name = "nora-inject"
version = "0.1.0"
edition = "2021"
authors = ["Floorp Team"]
description = "High-performance code injection system for browser customization"
license = "MPL-2.0"
repository = "https://github.com/Floorp-Projects/Floorp"

[lib]
crate-type = ["cdylib", "rlib"]

[dependencies]
# WebAssembly support
wasm-bindgen = "0.2"
js-sys = "0.3"
web-sys = "0.3"

# Serialization
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"

# File operations
walkdir = "2.0"
regex = "1.0"

# Error handling
thiserror = "1.0"
anyhow = "1.0"

# Async support
tokio = { version = "1.0", features = ["full"] }
futures = "0.3"

# Logging
log = "0.4"
env_logger = "0.10"

# Memory management
bytes = "1.0"

# String processing
memchr = "2.0"

[dependencies.wit-bindgen]
version = "0.13"
default-features = false
features = ["macros"]

[target.'cfg(target_arch = "wasm32")'.dependencies]
wasm-bindgen-futures = "0.4"
console_error_panic_hook = "0.1"

[dev-dependencies]
criterion = "0.5"
tempfile = "3.0"
tokio-test = "0.4"

[[bench]]
name = "performance"
harness = false

[profile.release]
opt-level = 3
lto = true
codegen-units = 1
panic = "abort"

[profile.release-wasm]
inherits = "release"
opt-level = "s"
```

## Main Rust Implementation

### 1. Library Entry Point (lib.rs)

```rust
//! Nora-Inject: High-performance code injection system
//! 
//! This library provides safe and efficient code injection capabilities
//! for browser customization, built with Rust and WebAssembly.

use wasm_bindgen::prelude::*;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

// Re-export main modules
pub mod inject;
pub mod parser;
pub mod filesystem;
pub mod validation;
pub mod error;
pub mod utils;

// Export main types
pub use inject::{InjectionEngine, InjectionConfig, InjectionResult};
pub use parser::{FileParser, ParsedFile, CodeBlock};
pub use error::{NoraError, Result};

// WebAssembly bindings
wit_bindgen::generate!({
    world: "nora-inject",
    path: "wit/world.wit",
});

/// Main injection configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
#[wasm_bindgen]
pub struct Config {
    /// Target directory for injection
    pub target_dir: String,
    /// Source files to inject
    pub source_files: Vec<String>,
    /// Injection patterns
    pub patterns: HashMap<String, String>,
    /// Validation rules
    pub validation: ValidationConfig,
    /// Performance settings
    pub performance: PerformanceConfig,
}

/// Validation configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ValidationConfig {
    /// Enable syntax validation
    pub validate_syntax: bool,
    /// Enable security checks
    pub security_checks: bool,
    /// Maximum file size (bytes)
    pub max_file_size: usize,
    /// Allowed file extensions
    pub allowed_extensions: Vec<String>,
}

/// Performance configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceConfig {
    /// Number of worker threads
    pub worker_threads: usize,
    /// Buffer size for file operations
    pub buffer_size: usize,
    /// Enable parallel processing
    pub parallel_processing: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            target_dir: String::new(),
            source_files: Vec::new(),
            patterns: HashMap::new(),
            validation: ValidationConfig {
                validate_syntax: true,
                security_checks: true,
                max_file_size: 10 * 1024 * 1024, // 10MB
                allowed_extensions: vec![
                    "js".to_string(),
                    "ts".to_string(),
                    "jsx".to_string(),
                    "tsx".to_string(),
                    "css".to_string(),
                    "html".to_string(),
                    "xhtml".to_string(),
                    "xml".to_string(),
                ],
            },
            performance: PerformanceConfig {
                worker_threads: num_cpus::get(),
                buffer_size: 64 * 1024, // 64KB
                parallel_processing: true,
            },
        }
    }
}

/// Main injection function exposed to WebAssembly
#[wasm_bindgen]
pub async fn inject_code(config_json: &str) -> Result<String, JsValue> {
    // Initialize panic hook for better error messages
    console_error_panic_hook::set_once();
    
    // Parse configuration
    let config: Config = serde_json::from_str(config_json)
        .map_err(|e| JsValue::from_str(&format!("Config parse error: {}", e)))?;
    
    // Create injection engine
    let mut engine = InjectionEngine::new(config)
        .map_err(|e| JsValue::from_str(&format!("Engine creation error: {}", e)))?;
    
    // Perform injection
    let result = engine.inject_all().await
        .map_err(|e| JsValue::from_str(&format!("Injection error: {}", e)))?;
    
    // Serialize result
    serde_json::to_string(&result)
        .map_err(|e| JsValue::from_str(&format!("Result serialization error: {}", e)))
}

/// Validate files without injection
#[wasm_bindgen]
pub async fn validate_files(config_json: &str) -> Result<String, JsValue> {
    let config: Config = serde_json::from_str(config_json)
        .map_err(|e| JsValue::from_str(&format!("Config parse error: {}", e)))?;
    
    let engine = InjectionEngine::new(config)
        .map_err(|e| JsValue::from_str(&format!("Engine creation error: {}", e)))?;
    
    let validation_result = engine.validate_all().await
        .map_err(|e| JsValue::from_str(&format!("Validation error: {}", e)))?;
    
    serde_json::to_string(&validation_result)
        .map_err(|e| JsValue::from_str(&format!("Result serialization error: {}", e)))
}

/// Get system information
#[wasm_bindgen]
pub fn get_system_info() -> String {
    let info = SystemInfo {
        version: env!("CARGO_PKG_VERSION").to_string(),
        build_time: env!("BUILD_TIME").to_string(),
        target_arch: std::env::consts::ARCH.to_string(),
        target_os: std::env::consts::OS.to_string(),
    };
    
    serde_json::to_string(&info).unwrap_or_default()
}

#[derive(Serialize)]
struct SystemInfo {
    version: String,
    build_time: String,
    target_arch: String,
    target_os: String,
}

// Initialize logging for WASM
#[wasm_bindgen(start)]
pub fn init() {
    env_logger::init();
    log::info!("Nora-Inject initialized");
}
```

### 2. Core Injection Logic (inject.rs)

```rust
//! Core injection engine implementation

use crate::{
    parser::{FileParser, ParsedFile},
    filesystem::FileSystem,
    validation::Validator,
    error::{NoraError, Result},
    Config,
};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use tokio::task::JoinSet;

/// Main injection engine
pub struct InjectionEngine {
    config: Config,
    parser: FileParser,
    filesystem: FileSystem,
    validator: Validator,
}

/// Injection configuration for a specific operation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InjectionConfig {
    /// Source file path
    pub source: PathBuf,
    /// Target file path
    pub target: PathBuf,
    /// Injection strategy
    pub strategy: InjectionStrategy,
    /// Custom patterns for this injection
    pub patterns: HashMap<String, String>,
}

/// Injection strategy
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum InjectionStrategy {
    /// Replace entire file
    Replace,
    /// Append to end of file
    Append,
    /// Prepend to beginning of file
    Prepend,
    /// Insert at specific pattern
    Insert { pattern: String, position: InsertPosition },
    /// Replace specific pattern
    ReplacePattern { pattern: String, replacement: String },
    /// Merge with existing content
    Merge { strategy: MergeStrategy },
}

/// Insert position relative to pattern
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum InsertPosition {
    Before,
    After,
    Replace,
}

/// Merge strategy for combining files
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MergeStrategy {
    /// Merge JavaScript/TypeScript imports
    ImportMerge,
    /// Merge CSS rules
    CssMerge,
    /// Merge XML/HTML elements
    XmlMerge,
    /// Custom merge function
    Custom { function: String },
}

/// Result of injection operation
#[derive(Debug, Serialize, Deserialize)]
pub struct InjectionResult {
    /// Number of files processed
    pub files_processed: usize,
    /// Number of successful injections
    pub successful_injections: usize,
    /// Number of failed injections
    pub failed_injections: usize,
    /// Detailed results for each file
    pub file_results: Vec<FileInjectionResult>,
    /// Performance metrics
    pub metrics: PerformanceMetrics,
}

/// Result for individual file injection
#[derive(Debug, Serialize, Deserialize)]
pub struct FileInjectionResult {
    /// Source file path
    pub source: String,
    /// Target file path
    pub target: String,
    /// Success status
    pub success: bool,
    /// Error message if failed
    pub error: Option<String>,
    /// Bytes written
    pub bytes_written: usize,
    /// Processing time in milliseconds
    pub processing_time_ms: u64,
}

/// Performance metrics
#[derive(Debug, Serialize, Deserialize)]
pub struct PerformanceMetrics {
    /// Total processing time
    pub total_time_ms: u64,
    /// Average time per file
    pub avg_time_per_file_ms: f64,
    /// Total bytes processed
    pub total_bytes: usize,
    /// Processing speed (bytes/second)
    pub bytes_per_second: f64,
}

impl InjectionEngine {
    /// Create new injection engine
    pub fn new(config: Config) -> Result<Self> {
        let parser = FileParser::new(&config.validation)?;
        let filesystem = FileSystem::new(&config.performance)?;
        let validator = Validator::new(&config.validation)?;
        
        Ok(Self {
            config,
            parser,
            filesystem,
            validator,
        })
    }
    
    /// Inject all configured files
    pub async fn inject_all(&mut self) -> Result<InjectionResult> {
        let start_time = std::time::Instant::now();
        let mut results = Vec::new();
        let mut total_bytes = 0;
        
        // Create injection configurations
        let injection_configs = self.create_injection_configs()?;
        
        if self.config.performance.parallel_processing {
            // Parallel processing
            results = self.process_parallel(injection_configs).await?;
        } else {
            // Sequential processing
            for config in injection_configs {
                let result = self.inject_single(config).await?;
                total_bytes += result.bytes_written;
                results.push(result);
            }
        }
        
        let total_time = start_time.elapsed();
        let successful = results.iter().filter(|r| r.success).count();
        let failed = results.len() - successful;
        
        Ok(InjectionResult {
            files_processed: results.len(),
            successful_injections: successful,
            failed_injections: failed,
            file_results: results,
            metrics: PerformanceMetrics {
                total_time_ms: total_time.as_millis() as u64,
                avg_time_per_file_ms: if results.is_empty() {
                    0.0
                } else {
                    total_time.as_millis() as f64 / results.len() as f64
                },
                total_bytes,
                bytes_per_second: if total_time.as_secs_f64() > 0.0 {
                    total_bytes as f64 / total_time.as_secs_f64()
                } else {
                    0.0
                },
            },
        })
    }
    
    /// Process injections in parallel
    async fn process_parallel(
        &mut self,
        configs: Vec<InjectionConfig>,
    ) -> Result<Vec<FileInjectionResult>> {
        let mut join_set = JoinSet::new();
        let chunk_size = std::cmp::max(1, configs.len() / self.config.performance.worker_threads);
        
        // Split work into chunks
        for chunk in configs.chunks(chunk_size) {
            let chunk = chunk.to_vec();
            let parser = self.parser.clone();
            let filesystem = self.filesystem.clone();
            let validator = self.validator.clone();
            
            join_set.spawn(async move {
                let mut results = Vec::new();
                for config in chunk {
                    match Self::inject_single_static(config, &parser, &filesystem, &validator).await {
                        Ok(result) => results.push(result),
                        Err(e) => {
                            log::error!("Injection failed: {}", e);
                            // Create error result
                            results.push(FileInjectionResult {
                                source: "unknown".to_string(),
                                target: "unknown".to_string(),
                                success: false,
                                error: Some(e.to_string()),
                                bytes_written: 0,
                                processing_time_ms: 0,
                            });
                        }
                    }
                }
                results
            });
        }
        
        // Collect results
        let mut all_results = Vec::new();
        while let Some(chunk_results) = join_set.join_next().await {
            match chunk_results {
                Ok(results) => all_results.extend(results),
                Err(e) => {
                    log::error!("Task join error: {}", e);
                    return Err(NoraError::ProcessingError(format!("Task join error: {}", e)));
                }
            }
        }
        
        Ok(all_results)
    }
    
    /// Inject single file
    async fn inject_single(&mut self, config: InjectionConfig) -> Result<FileInjectionResult> {
        Self::inject_single_static(config, &self.parser, &self.filesystem, &self.validator).await
    }
    
    /// Static version for parallel processing
    async fn inject_single_static(
        config: InjectionConfig,
        parser: &FileParser,
        filesystem: &FileSystem,
        validator: &Validator,
    ) -> Result<FileInjectionResult> {
        let start_time = std::time::Instant::now();
        
        // Validate source file
        validator.validate_file(&config.source).await?;
        
        // Parse source file
        let parsed = parser.parse_file(&config.source).await?;
        
        // Read target file if it exists
        let target_content = if config.target.exists() {
            filesystem.read_file(&config.target).await?
        } else {
            String::new()
        };
        
        // Apply injection strategy
        let injected_content = apply_injection_strategy(
            &config.strategy,
            &parsed.content,
            &target_content,
            &config.patterns,
        )?;
        
        // Write result
        filesystem.write_file(&config.target, &injected_content).await?;
        
        let processing_time = start_time.elapsed();
        
        Ok(FileInjectionResult {
            source: config.source.to_string_lossy().to_string(),
            target: config.target.to_string_lossy().to_string(),
            success: true,
            error: None,
            bytes_written: injected_content.len(),
            processing_time_ms: processing_time.as_millis() as u64,
        })
    }
    
    /// Create injection configurations from main config
    fn create_injection_configs(&self) -> Result<Vec<InjectionConfig>> {
        let mut configs = Vec::new();
        
        for source_file in &self.config.source_files {
            let source = PathBuf::from(source_file);
            let target = PathBuf::from(&self.config.target_dir)
                .join(source.file_name().ok_or_else(|| {
                    NoraError::ConfigError(format!("Invalid source file: {}", source_file))
                })?);
            
            configs.push(InjectionConfig {
                source,
                target,
                strategy: InjectionStrategy::Replace, // Default strategy
                patterns: self.config.patterns.clone(),
            });
        }
        
        Ok(configs)
    }
    
    /// Validate all files without injection
    pub async fn validate_all(&self) -> Result<ValidationResult> {
        let mut results = Vec::new();
        
        for source_file in &self.config.source_files {
            let source = PathBuf::from(source_file);
            let result = self.validator.validate_file(&source).await;
            
            results.push(FileValidationResult {
                file: source_file.clone(),
                valid: result.is_ok(),
                error: result.err().map(|e| e.to_string()),
            });
        }
        
        Ok(ValidationResult { files: results })
    }
}

/// Apply injection strategy to content
fn apply_injection_strategy(
    strategy: &InjectionStrategy,
    source_content: &str,
    target_content: &str,
    patterns: &HashMap<String, String>,
) -> Result<String> {
    match strategy {
        InjectionStrategy::Replace => Ok(source_content.to_string()),
        
        InjectionStrategy::Append => {
            Ok(format!("{}\n{}", target_content, source_content))
        }
        
        InjectionStrategy::Prepend => {
            Ok(format!("{}\n{}", source_content, target_content))
        }
        
        InjectionStrategy::Insert { pattern, position } => {
            insert_at_pattern(source_content, target_content, pattern, position)
        }
        
        InjectionStrategy::ReplacePattern { pattern, replacement } => {
            let regex = regex::Regex::new(pattern)
                .map_err(|e| NoraError::PatternError(format!("Invalid regex: {}", e)))?;
            Ok(regex.replace_all(target_content, replacement.as_str()).to_string())
        }
        
        InjectionStrategy::Merge { strategy } => {
            merge_content(source_content, target_content, strategy)
        }
    }
}

/// Insert content at specific pattern
fn insert_at_pattern(
    source_content: &str,
    target_content: &str,
    pattern: &str,
    position: &InsertPosition,
) -> Result<String> {
    let regex = regex::Regex::new(pattern)
        .map_err(|e| NoraError::PatternError(format!("Invalid regex: {}", e)))?;
    
    if let Some(mat) = regex.find(target_content) {
        match position {
            InsertPosition::Before => {
                let (before, after) = target_content.split_at(mat.start());
                Ok(format!("{}{}\n{}", before, source_content, after))
            }
            InsertPosition::After => {
                let (before, after) = target_content.split_at(mat.end());
                Ok(format!("{}\n{}{}", before, source_content, after))
            }
            InsertPosition::Replace => {
                Ok(regex.replace(target_content, source_content).to_string())
            }
        }
    } else {
        Err(NoraError::PatternError(format!("Pattern not found: {}", pattern)))
    }
}

/// Merge content using specific strategy
fn merge_content(
    source_content: &str,
    target_content: &str,
    strategy: &MergeStrategy,
) -> Result<String> {
    match strategy {
        MergeStrategy::ImportMerge => merge_js_imports(source_content, target_content),
        MergeStrategy::CssMerge => merge_css_rules(source_content, target_content),
        MergeStrategy::XmlMerge => merge_xml_elements(source_content, target_content),
        MergeStrategy::Custom { function } => {
            // This would call a custom merge function
            // For now, just append
            Ok(format!("{}\n{}", target_content, source_content))
        }
    }
}

/// Merge JavaScript/TypeScript imports
fn merge_js_imports(source_content: &str, target_content: &str) -> Result<String> {
    // Extract imports from both files
    let import_regex = regex::Regex::new(r"^import\s+.*?;?\s*$")
        .map_err(|e| NoraError::PatternError(format!("Import regex error: {}", e)))?;
    
    let mut imports = std::collections::HashSet::new();
    let mut target_without_imports = String::new();
    let mut source_without_imports = String::new();
    
    // Process target content
    for line in target_content.lines() {
        if import_regex.is_match(line) {
            imports.insert(line.to_string());
        } else {
            target_without_imports.push_str(line);
            target_without_imports.push('\n');
        }
    }
    
    // Process source content
    for line in source_content.lines() {
        if import_regex.is_match(line) {
            imports.insert(line.to_string());
        } else {
            source_without_imports.push_str(line);
            source_without_imports.push('\n');
        }
    }
    
    // Combine imports and content
    let mut result = String::new();
    for import in imports {
        result.push_str(&import);
        result.push('\n');
    }
    result.push('\n');
    result.push_str(&target_without_imports);
    result.push_str(&source_without_imports);
    
    Ok(result)
}

/// Merge CSS rules
fn merge_css_rules(source_content: &str, target_content: &str) -> Result<String> {
    // Simple concatenation for CSS - could be more sophisticated
    Ok(format!("{}\n\n{}", target_content, source_content))
}

/// Merge XML elements
fn merge_xml_elements(source_content: &str, target_content: &str) -> Result<String> {
    // Simple concatenation for XML - could parse and merge properly
    Ok(format!("{}\n{}", target_content, source_content))
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ValidationResult {
    pub files: Vec<FileValidationResult>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FileValidationResult {
    pub file: String,
    pub valid: bool,
    pub error: Option<String>,
}
```

## WebAssembly Interface Types (wit/world.wit)

```wit
// WebAssembly Interface Types for Nora-Inject

/// Main world interface for Nora-Inject
world nora-inject {
    /// Import host functions
    import wasi:filesystem/types@0.2.0
    import wasi:filesystem/preopens@0.2.0
    
    /// Export main functions
    export inject-code: func(config: string) -> result<string, string>
    export validate-files: func(config: string) -> result<string, string>
    export get-system-info: func() -> string
    
    /// Configuration types
    record injection-config {
        target-dir: string,
        source-files: list<string>,
        patterns: list<pattern-pair>,
        validation: validation-config,
        performance: performance-config,
    }
    
    record pattern-pair {
        key: string,
        value: string,
    }
    
    record validation-config {
        validate-syntax: bool,
        security-checks: bool,
        max-file-size: u64,
        allowed-extensions: list<string>,
    }
    
    record performance-config {
        worker-threads: u32,
        buffer-size: u32,
        parallel-processing: bool,
    }
    
    /// Result types
    record injection-result {
        files-processed: u32,
        successful-injections: u32,
        failed-injections: u32,
        file-results: list<file-injection-result>,
        metrics: performance-metrics,
    }
    
    record file-injection-result {
        source: string,
        target: string,
        success: bool,
        error: option<string>,
        bytes-written: u64,
        processing-time-ms: u64,
    }
    
    record performance-metrics {
        total-time-ms: u64,
        avg-time-per-file-ms: float64,
        total-bytes: u64,
        bytes-per-second: float64,
    }
    
    /// Error types
    variant nora-error {
        io-error(string),
        parse-error(string),
        validation-error(string),
        pattern-error(string),
        config-error(string),
        processing-error(string),
    }
}
```

## TypeScript Integration

### 1. TypeScript Bindings (generated)

```typescript
// Generated TypeScript bindings for Nora-Inject WASM module

export interface InjectionConfig {
  targetDir: string;
  sourceFiles: string[];
  patterns: Record<string, string>;
  validation: ValidationConfig;
  performance: PerformanceConfig;
}

export interface ValidationConfig {
  validateSyntax: boolean;
  securityChecks: boolean;
  maxFileSize: number;
  allowedExtensions: string[];
}

export interface PerformanceConfig {
  workerThreads: number;
  bufferSize: number;
  parallelProcessing: boolean;
}

export interface InjectionResult {
  filesProcessed: number;
  successfulInjections: number;
  failedInjections: number;
  fileResults: FileInjectionResult[];
  metrics: PerformanceMetrics;
}

export interface FileInjectionResult {
  source: string;
  target: string;
  success: boolean;
  error?: string;
  bytesWritten: number;
  processingTimeMs: number;
}

export interface PerformanceMetrics {
  totalTimeMs: number;
  avgTimePerFileMs: number;
  totalBytes: number;
  bytesPerSecond: number;
}

// WASM module interface
export interface NoraInjectModule {
  inject_code(configJson: string): Promise<string>;
  validate_files(configJson: string): Promise<string>;
  get_system_info(): string;
}

// Load WASM module
export async function loadNoraInject(): Promise<NoraInjectModule> {
  const wasmModule = await import('./nora_inject_lib.wasm');
  return wasmModule;
}
```

### 2. High-level TypeScript API

```typescript
// High-level TypeScript API for Nora-Inject

import { loadNoraInject, InjectionConfig, InjectionResult } from './bindings';

export class NoraInject {
  private wasmModule: any;
  private initialized = false;

  async initialize(): Promise<void> {
    if (!this.initialized) {
      this.wasmModule = await loadNoraInject();
      this.initialized = true;
    }
  }

  async injectCode(config: InjectionConfig): Promise<InjectionResult> {
    await this.initialize();
    
    const configJson = JSON.stringify(config);
    const resultJson = await this.wasmModule.inject_code(configJson);
    
    return JSON.parse(resultJson) as InjectionResult;
  }

  async validateFiles(config: InjectionConfig): Promise<ValidationResult> {
    await this.initialize();
    
    const configJson = JSON.stringify(config);
    const resultJson = await this.wasmModule.validate_files(configJson);
    
    return JSON.parse(resultJson) as ValidationResult;
  }

  getSystemInfo(): SystemInfo {
    if (!this.initialized) {
      throw new Error('NoraInject not initialized');
    }
    
    const infoJson = this.wasmModule.get_system_info();
    return JSON.parse(infoJson) as SystemInfo;
  }
}

// Convenience functions
export async function injectFiles(
  sourceFiles: string[],
  targetDir: string,
  options: Partial<InjectionConfig> = {}
): Promise<InjectionResult> {
  const nora = new NoraInject();
  
  const config: InjectionConfig = {
    targetDir,
    sourceFiles,
    patterns: {},
    validation: {
      validateSyntax: true,
      securityChecks: true,
      maxFileSize: 10 * 1024 * 1024, // 10MB
      allowedExtensions: ['js', 'ts', 'jsx', 'tsx', 'css', 'html'],
    },
    performance: {
      workerThreads: navigator.hardwareConcurrency || 4,
      bufferSize: 64 * 1024, // 64KB
      parallelProcessing: true,
    },
    ...options,
  };
  
  return nora.injectCode(config);
}

export async function validateSourceFiles(
  sourceFiles: string[],
  options: Partial<ValidationConfig> = {}
): Promise<ValidationResult> {
  const nora = new NoraInject();
  
  const config: InjectionConfig = {
    targetDir: '',
    sourceFiles,
    patterns: {},
    validation: {
      validateSyntax: true,
      securityChecks: true,
      maxFileSize: 10 * 1024 * 1024,
      allowedExtensions: ['js', 'ts', 'jsx', 'tsx', 'css', 'html'],
      ...options,
    },
    performance: {
      workerThreads: 1,
      bufferSize: 64 * 1024,
      parallelProcessing: false,
    },
  };
  
  return nora.validateFiles(config);
}
```

## Usage in Build Scripts

### 1. Integration with Main Build System

```typescript
// Integration with Floorp's main build system

import { NoraInject, injectFiles } from '../crates/nora-inject';
import * as path from 'path';

export async function injectCustomCode(
  binDir: string,
  sourceDir: string = '_dist/noraneko'
): Promise<void> {
  console.log('Starting code injection with Nora-Inject...');
  
  try {
    // Define source files to inject
    const sourceFiles = [
      path.join(sourceDir, 'content/main.js'),
      path.join(sourceDir, 'content/startup.js'),
      path.join(sourceDir, 'content/modules.js'),
      path.join(sourceDir, 'chrome/browser.css'),
      path.join(sourceDir, 'chrome/userChrome.css'),
    ];
    
    // Configure injection
    const result = await injectFiles(sourceFiles, binDir, {
      patterns: {
        // Replace placeholders with actual values
        '__FLOORP_VERSION__': process.env.FLOORP_VERSION || 'dev',
        '__BUILD_DATE__': new Date().toISOString(),
        '__DEBUG_MODE__': process.env.NODE_ENV === 'development' ? 'true' : 'false',
      },
      validation: {
        validateSyntax: true,
        securityChecks: true,
        maxFileSize: 50 * 1024 * 1024, // 50MB for large files
        allowedExtensions: ['js', 'ts', 'css', 'html', 'xhtml', 'xml'],
      },
      performance: {
        workerThreads: Math.min(8, require('os').cpus().length),
        bufferSize: 128 * 1024, // 128KB
        parallelProcessing: true,
      },
    });
    
    // Log results
    console.log(`Injection completed:`);
    console.log(`  Files processed: ${result.filesProcessed}`);
    console.log(`  Successful: ${result.successfulInjections}`);
    console.log(`  Failed: ${result.failedInjections}`);
    console.log(`  Total time: ${result.metrics.totalTimeMs}ms`);
    console.log(`  Speed: ${(result.metrics.bytesPerSecond / 1024 / 1024).toFixed(2)} MB/s`);
    
    // Handle failures
    if (result.failedInjections > 0) {
      console.error('Some injections failed:');
      result.fileResults
        .filter(r => !r.success)
        .forEach(r => console.error(`  ${r.source}: ${r.error}`));
      
      if (process.env.NODE_ENV === 'production') {
        throw new Error(`${result.failedInjections} injection(s) failed`);
      }
    }
    
  } catch (error) {
    console.error('Code injection failed:', error);
    throw error;
  }
}

// Validation function for CI/CD
export async function validateCodeIntegrity(sourceDir: string): Promise<boolean> {
  console.log('Validating code integrity...');
  
  try {
    const nora = new NoraInject();
    
    // Get all source files
    const sourceFiles = await getAllSourceFiles(sourceDir);
    
    const result = await nora.validateFiles({
      targetDir: '',
      sourceFiles,
      patterns: {},
      validation: {
        validateSyntax: true,
        securityChecks: true,
        maxFileSize: 100 * 1024 * 1024, // 100MB
        allowedExtensions: ['js', 'ts', 'jsx', 'tsx', 'css', 'html', 'xhtml', 'xml'],
      },
      performance: {
        workerThreads: 1,
        bufferSize: 64 * 1024,
        parallelProcessing: false,
      },
    });
    
    const invalidFiles = result.files.filter(f => !f.valid);
    
    if (invalidFiles.length > 0) {
      console.error('Invalid files found:');
      invalidFiles.forEach(f => console.error(`  ${f.file}: ${f.error}`));
      return false;
    }
    
    console.log(`All ${result.files.length} files validated successfully`);
    return true;
    
  } catch (error) {
    console.error('Validation failed:', error);
    return false;
  }
}

async function getAllSourceFiles(dir: string): Promise<string[]> {
  const { glob } = await import('glob');
  
  return glob('**/*.{js,ts,jsx,tsx,css,html,xhtml,xml}', {
    cwd: dir,
    absolute: true,
  });
}
```

## Performance Benchmarks

```rust
// Performance benchmarks for Nora-Inject

use criterion::{criterion_group, criterion_main, Criterion, BenchmarkId};
use nora_inject::{InjectionEngine, Config};
use std::path::PathBuf;
use tempfile::TempDir;

fn bench_injection_performance(c: &mut Criterion) {
    let mut group = c.benchmark_group("injection_performance");
    
    // Test different file sizes
    let file_sizes = vec![1024, 10240, 102400, 1024000]; // 1KB to 1MB
    
    for size in file_sizes {
        group.bench_with_input(
            BenchmarkId::new("single_file", size),
            &size,
            |b, &size| {
                let temp_dir = TempDir::new().unwrap();
                let source_content = "a".repeat(size);
                let source_file = temp_dir.path().join("source.js");
                std::fs::write(&source_file, source_content).unwrap();
                
                let config = Config {
                    target_dir: temp_dir.path().to_string_lossy().to_string(),
                    source_files: vec![source_file.to_string_lossy().to_string()],
                    ..Default::default()
                };
                
                b.iter(|| {
                    let rt = tokio::runtime::Runtime::new().unwrap();
                    rt.block_on(async {
                        let mut engine = InjectionEngine::new(config.clone()).unwrap();
                        engine.inject_all().await.unwrap()
                    })
                });
            },
        );
    }
    
    group.finish();
}

fn bench_parallel_vs_sequential(c: &mut Criterion) {
    let mut group = c.benchmark_group("parallel_vs_sequential");
    
    let temp_dir = TempDir::new().unwrap();
    let file_count = 100;
    let file_size = 10240; // 10KB each
    
    // Create test files
    let mut source_files = Vec::new();
    for i in 0..file_count {
        let content = format!("// File {}\n{}", i, "a".repeat(file_size));
        let file_path = temp_dir.path().join(format!("source_{}.js", i));
        std::fs::write(&file_path, content).unwrap();
        source_files.push(file_path.to_string_lossy().to_string());
    }
    
    // Sequential benchmark
    group.bench_function("sequential", |b| {
        let config = Config {
            target_dir: temp_dir.path().to_string_lossy().to_string(),
            source_files: source_files.clone(),
            performance: nora_inject::PerformanceConfig {
                parallel_processing: false,
                ..Default::default()
            },
            ..Default::default()
        };
        
        b.iter(|| {
            let rt = tokio::runtime::Runtime::new().unwrap();
            rt.block_on(async {
                let mut engine = InjectionEngine::new(config.clone()).unwrap();
                engine.inject_all().await.unwrap()
            })
        });
    });
    
    // Parallel benchmark
    group.bench_function("parallel", |b| {
        let config = Config {
            target_dir: temp_dir.path().to_string_lossy().to_string(),
            source_files: source_files.clone(),
            performance: nora_inject::PerformanceConfig {
                parallel_processing: true,
                worker_threads: num_cpus::get(),
                ..Default::default()
            },
            ..Default::default()
        };
        
        b.iter(|| {
            let rt = tokio::runtime::Runtime::new().unwrap();
            rt.block_on(async {
                let mut engine = InjectionEngine::new(config.clone()).unwrap();
                engine.inject_all().await.unwrap()
            })
        });
    });
    
    group.finish();
}

criterion_group!(benches, bench_injection_performance, bench_parallel_vs_sequential);
criterion_main!(benches);
```

The Nora-Inject system provides Floorp with a powerful, safe, and high-performance code injection capability that leverages Rust's performance and safety guarantees while maintaining a convenient TypeScript interface for build scripts. This enables efficient customization of Firefox binaries while ensuring security and reliability.