# Noraneko Development Guide

Welcome to the Noraneko development guide! This document provides step-by-step instructions for setting up, building, and developing the Noraneko browser.

## 🚀 Quick Start

### Prerequisites

- **Deno**: Version 1.40+ ([Install Deno](https://deno.land/manual/getting_started/installation))
- **Node.js**: Version 18+ (for some development tools)
- **Git**: For version control
- **Visual Studio Build Tools** (Windows) or **Xcode** (macOS) or **GCC** (Linux)

### 1. Clone the Repository

```bash
git clone https://github.com/nyanrus/noraneko.git
cd noraneko
```

### 2. Initial Setup

```bash
# Download Mozilla source and prepare development environment
deno run -A tools/dev/index.ts prepare
```

### 3. Development Build

```bash
# Build for development (includes HMR and source maps)
deno run -A build.ts --dev
```

### 4. Launch Development Browser

```bash
# Launch the development browser
deno run -A tools/dev/index.ts start
```

## 🛠️ Development Workflow

### Project Structure Overview

```text
noraneko/
├── src/                    # 🛠️ Source Code
│   ├── core/              # Core browser functionality
│   ├── features/          # Browser features
│   ├── ui/                # User interface components
│   ├── themes/            # Visual themes
│   └── shared/            # Shared utilities
├── tools/                 # 🔧 Development Tools
│   ├── build/             # Build system
│   └── dev/               # Development utilities
├── docs/                  # 📚 Documentation
└── _dist/                 # 📦 Build output
```

### Development Modes

#### 1. **Full Development Build**

```bash
# Complete build including Mozilla Firefox base
deno run -A build.ts --dev
```

This mode:

- Downloads and builds Mozilla Firefox
- Applies Noraneko patches and features
- Enables hot module reloading (HMR)
- Includes source maps for debugging

#### 2. **Quick Development Build**

```bash
# Skip Mozilla build (faster iteration)
deno run -A build.ts --dev-skip-mozbuild
```

This mode:

- Skips Mozilla build (requires previous full build)
- Only rebuilds Noraneko-specific code
- Much faster for feature development

#### 3. **Production Build**

```bash
# Production build with optimizations
deno run -A build.ts --production
```

This mode:

- Optimized and minified builds
- No source maps
- Production-ready output

## 🎯 Feature Development

### Adding a New Feature

1. **Create Feature Directory**

   ```bash
   mkdir src/features/chrome/my-new-feature
   cd src/features/chrome/my-new-feature
   ```

2. **Create Feature Files**

   ```typescript
   // src/features/chrome/my-new-feature/index.ts
   export class MyNewFeature {
     static initialize() {
       console.log("My new feature initialized!");
     }
   }
   ```

3. **Register Feature**

   Add your feature to the appropriate loader in `src/core/glue/`.

4. **Test Your Feature**

   ```bash
   # Rebuild and test
   deno run -A build.ts --dev-skip-mozbuild
   deno run -A tools/dev/index.ts start
   ```

### Feature Development Guidelines

- **Self-contained**: Keep features in their own directories
- **Clear naming**: Use descriptive names for files and classes
- **Documentation**: Add comments and documentation
- **Testing**: Include tests when applicable

## 🎨 UI Development

### Working with Themes

```bash
# Navigate to themes directory
cd src/themes/

# Available themes:
# - noraneko/    (Default theme)
# - fluerial/    (Fluerial theme)
# - lepton/      (Lepton theme integration)
```

### Creating Custom UI

```bash
# Navigate to UI directory
cd src/ui/

# Add new UI components:
# - about-pages/  (about:* pages)
# - settings/     (Settings pages)
```

## 🔧 Build System

### Build Architecture

The build system is organized into phases:

1. **Pre-Build Phase**: Preparation and setup
2. **Mozilla Build Phase**: Core Firefox build
3. **Post-Build Phase**: Noraneko integration

### Build Commands

```bash
# View all build options
deno run -A build.ts --help

# Clean build output
deno run -A tools/dev/index.ts clean

# Reset development environment
deno run -A tools/dev/index.ts reset
```

### Build Configuration

Build settings are centralized in `tools/build/defines.ts`:

- Platform-specific paths
- Development vs production settings
- Port configurations
- Feature flags

## 🧪 Testing

### Running Tests

```bash
# Run all tests
deno run -A tools/dev/index.ts test

# Run specific test suite
deno run -A tests/unit/my-feature.test.ts
```

### Writing Tests

Create test files in the `tests/` directory:

```typescript
// tests/unit/my-feature.test.ts
import { assertEquals } from "https://deno.land/std/testing/asserts.ts";
import { MyNewFeature } from "../../src/features/chrome/my-new-feature/index.ts";

Deno.test("MyNewFeature initializes correctly", () => {
  // Test implementation
  assertEquals(typeof MyNewFeature.initialize, "function");
});
```

## 🔍 Debugging

### Development Tools

- **Browser DevTools**: Standard web debugging tools
- **Source Maps**: Available in development builds
- **Logging**: Built-in logging system in `tools/build/logger.ts`

### Common Issues

#### Build Failures

```bash
# Clean and rebuild
deno run -A tools/dev/index.ts clean
deno run -A build.ts --dev
```

#### Module Loading Issues

- Check import paths in your features
- Verify feature registration in loaders
- Check browser console for errors

#### Performance Issues

- Use `--dev-skip-mozbuild` for faster iterations
- Profile with browser DevTools
- Check build logs for optimization hints

## 📚 Advanced Topics

### Custom Build Tasks

Create custom build tasks in `tools/build/tasks/`:

```typescript
// tools/build/tasks/my-custom-task.ts
export async function myCustomTask() {
  console.log("Running my custom task...");
  // Task implementation
}
```

### Integration with External Tools

- **TypeScript**: Configured via `tsconfig.json`
- **ESLint**: Code linting and formatting
- **Vite**: For UI development and HMR

### Contributing Guidelines

1. **Fork the Repository**
2. **Create Feature Branch**: `git checkout -b feature/my-feature`
3. **Follow Coding Standards**: Use TypeScript, proper naming
4. **Add Tests**: Include tests for new features
5. **Update Documentation**: Update relevant docs
6. **Submit Pull Request**: Include clear description

## 🆘 Getting Help

### Resources

- **Documentation**: See `docs/` directory
- **Issues**: [GitHub Issues](https://github.com/nyanrus/noraneko/issues)
- **Discussions**: [GitHub Discussions](https://github.com/nyanrus/noraneko/discussions)

### Common Commands Reference

```bash
# Setup and build
deno run -A tools/dev/index.ts prepare
deno run -A build.ts --dev

# Development workflow
deno run -A build.ts --dev-skip-mozbuild
deno run -A tools/dev/index.ts start

# Maintenance
deno run -A tools/dev/index.ts clean
deno run -A tools/dev/index.ts reset

# Testing
deno run -A tools/dev/index.ts test
```

---

Happy coding! 🦊✨
