# Build System Improvements

## 🎯 Current Issues with Build System

1. **Complex build script** - Single large file handles multiple
   responsibilities
2. **Unclear dependencies** - Build steps are tightly coupled
3. **Difficult debugging** - Hard to understand build failures
4. **Poor error messages** - Generic error handling

## 🔧 Proposed Improvements

### 1. Modular Build System

Split `scripts/build.ts` into smaller, focused modules:

```
tools/build/
├── index.ts                    # Main build orchestrator
├── core/                       # Core build functionality
│   ├── environment.ts          # Environment setup
│   ├── binary.ts              # Binary management
│   └── validation.ts          # Build validation
├── phases/                     # Build phases
│   ├── before-mozbuild.ts     # Pre-mozbuild phase
│   ├── after-mozbuild.ts      # Post-mozbuild phase
│   └── development.ts         # Development phase
├── tasks/                      # Individual tasks
│   ├── manifest-injection.ts  # Manifest injection
│   ├── xhtml-injection.ts     # XHTML injection
│   └── symlink-management.ts  # Symlink creation
└── utils/                      # Build utilities
    ├── logger.ts              # Enhanced logging
    ├── path-resolver.ts       # Path resolution
    └── process-manager.ts     # Process management
```

### 2. Enhanced Configuration Management

Centralize all configuration:

```typescript
// config/build.config.ts
export interface BuildConfig {
  mode: "dev" | "test" | "release";
  platform: "windows" | "linux" | "darwin";
  paths: {
    bin: string;
    dist: string;
    temp: string;
  };
  ports: {
    main: number;
    settings: number;
    designs: number;
  };
  features: {
    hmr: boolean;
    sourcemaps: boolean;
    minify: boolean;
  };
}
```

### 3. Better Error Handling

```typescript
// tools/build/utils/error-handler.ts
export class BuildError extends Error {
  constructor(
    message: string,
    public phase: string,
    public suggestion?: string,
  ) {
    super(message);
    this.name = "BuildError";
  }
}

export function handleBuildError(error: BuildError) {
  console.error(`❌ Build failed in ${error.phase}:`);
  console.error(`   ${error.message}`);
  if (error.suggestion) {
    console.info(`💡 Suggestion: ${error.suggestion}`);
  }
}
```

### 4. Progress Reporting

Add clear progress indicators:

```typescript
// tools/build/utils/progress.ts
export class ProgressReporter {
  private steps: string[] = [];
  private current = 0;

  constructor(steps: string[]) {
    this.steps = steps;
  }

  start(step: string) {
    this.current++;
    console.log(`[${this.current}/${this.steps.length}] ${step}...`);
  }

  complete(step: string) {
    console.log(`✅ ${step} completed`);
  }

  fail(step: string, error: Error) {
    console.error(`❌ ${step} failed: ${error.message}`);
  }
}
```

## 🚀 Implementation Plan

### Phase 1: Refactor Core Build Logic

1. Extract environment setup
2. Separate build phases
3. Create task abstractions

### Phase 2: Improve Developer Experience

1. Add progress reporting
2. Enhance error messages
3. Create build templates

### Phase 3: Optimize Performance

1. Parallel task execution
2. Incremental builds
3. Caching improvements

## 📋 Configuration Examples

### Development Configuration

```typescript
export const devConfig: BuildConfig = {
  mode: "dev",
  platform: process.platform as any,
  paths: {
    bin: "_dist/bin",
    dist: "_dist",
    temp: "_dist/temp",
  },
  ports: {
    main: 5181,
    settings: 5183,
    designs: 5182,
  },
  features: {
    hmr: true,
    sourcemaps: true,
    minify: false,
  },
};
```

### Production Configuration

```typescript
export const prodConfig: BuildConfig = {
  mode: "release",
  platform: process.platform as any,
  paths: {
    bin: "_dist/bin",
    dist: "_dist",
    temp: "_dist/temp",
  },
  ports: {
    main: 5181,
    settings: 5183,
    designs: 5182,
  },
  features: {
    hmr: false,
    sourcemaps: false,
    minify: true,
  },
};
```
