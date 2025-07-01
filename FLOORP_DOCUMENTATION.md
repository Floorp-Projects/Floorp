# Floorp Browser - Comprehensive Directory Structure & Code Architecture Documentation

## Overview

Floorp is a Firefox-based browser built on Mozilla Firefox, designed to keep the Open, Private and Sustainable Web alive. This documentation provides a comprehensive overview of the project's directory structure, code architecture, and development workflow.

## Project Architecture

Floorp is built using a hybrid architecture combining:
- **TypeScript/JavaScript** for frontend applications and build tooling
- **Rust** for performance-critical components and WebAssembly modules
- **Deno** as the primary runtime and task runner
- **SolidJS** for reactive UI components
- **Mozilla Firefox/Gecko** as the underlying browser engine

## Root Directory Structure

```
floorp/
├── .git/                    # Git repository metadata
├── .github/                 # GitHub Actions workflows and templates
├── .vscode/                 # VSCode workspace configuration
├── .claude/                 # Claude AI assistant configuration
├── @types/                  # TypeScript type definitions
├── src/                     # Main source code directory
├── scripts/                 # Build and automation scripts
├── crates/                  # Rust crates and components
├── gecko/                   # Firefox/Gecko integration files
├── _dist/                   # Build output directory (generated)
├── build.ts                 # Main build script
├── deno.json               # Deno configuration and tasks
├── package.json            # Node.js dependencies
├── Cargo.toml              # Rust workspace configuration
├── tsconfig.json           # TypeScript configuration
├── moz.build               # Mozilla build system integration
└── README.md               # Project documentation
```

## Source Code Structure (`src/`)

### Applications (`src/apps/`)

The applications directory contains the main UI applications that make up Floorp:

```
src/apps/
├── main/                   # Main browser application
├── settings/               # Settings/preferences UI
├── newtab/                 # New tab page
├── notes/                  # Notes functionality
├── welcome/                # Welcome/onboarding screens
├── startup/                # Browser startup logic
├── modal-child/            # Modal dialog components
├── os/                     # OS-specific integrations
├── i18n-supports/          # Internationalization support
├── modules/                # Shared application modules
└── common/                 # Common utilities and components
```

#### Main Application (`src/apps/main/`)
- **Purpose**: Core browser functionality and UI
- **Technology**: SolidJS + TypeScript
- **Key Dependencies**: 
  - `@nora/solid-xul` - XUL integration layer
  - `@nora/skin` - Theming system
  - `solid-js` - Reactive UI framework
  - `tailwindcss` - CSS framework

#### Settings Application (`src/apps/settings/`)
- **Purpose**: Browser configuration and preferences
- **Technology**: Modern web technologies with shadcn/ui components
- **Build System**: Vite with TypeScript

#### New Tab Application (`src/apps/newtab/`)
- **Purpose**: Customizable new tab page
- **Technology**: SolidJS with modern web APIs
- **Features**: Likely includes bookmarks, shortcuts, and customization options

### Packages (`src/packages/`)

Reusable packages and libraries:

```
src/packages/
├── solid-xul/              # SolidJS integration with XUL
├── skin/                   # Theming and styling system
├── user-js-runner/         # User script execution engine
└── vitest-noraneko/        # Testing utilities
```

#### Solid-XUL (`src/packages/solid-xul/`)
- **Purpose**: Bridge between SolidJS and Firefox's XUL system
- **Key Files**:
  - `index.ts` - Main API exports
  - `jsx-runtime.ts` - JSX runtime for XUL elements
- **Architecture**: Provides React/SolidJS-like component model for XUL

#### Skin System (`src/packages/skin/`)
- **Purpose**: Theming and visual customization
- **Themes Available**:
  - `fluerial/` - Fluerial theme
  - `lepton/` - Lepton-based theme (Firefox UI Fix)
  - `noraneko/` - Default Noraneko theme

### Installers (`src/installers/`)

Platform-specific installation packages:

```
src/installers/
└── stub-win64-installer/   # Windows 64-bit installer (Tauri-based)
```

## Build System & Scripts (`scripts/`)

The scripts directory contains the build pipeline and automation tools:

```
scripts/
├── inject/                 # Code injection and patching
│   ├── manifest.ts         # Manifest injection
│   ├── xhtml.ts           # XHTML template injection
│   ├── mixin-loader.ts    # Mixin system loader
│   ├── mixins/            # UI mixins and modifications
│   ├── shared/            # Shared injection utilities
│   └── wasm/              # WebAssembly modules
├── launchDev/             # Development server scripts
├── update/                # Update and versioning scripts
├── cssUpdate/             # CSS processing and updates
├── git-patches/           # Git patch management
│   ├── git-patches-manager.ts
│   └── patches/           # Patch files
└── i18n/                  # Internationalization scripts
```

### Build Pipeline Architecture

1. **Binary Management**: Downloads and manages Firefox binaries
2. **Patch Application**: Applies custom patches to Firefox source
3. **Code Injection**: Injects custom UI and functionality
4. **Asset Processing**: Processes CSS, JavaScript, and other assets
5. **Development Server**: Hot-reload development environment

## Rust Components (`crates/`)

```
crates/
└── nora-inject/           # Rust-based code injection system
    ├── src/               # Rust source code
    ├── wit/               # WebAssembly Interface Types
    ├── Cargo.toml         # Rust package configuration
    └── *.wasm             # Compiled WebAssembly modules
```

### Nora-Inject Crate
- **Purpose**: High-performance code injection and transformation
- **Technology**: Rust compiled to WebAssembly
- **Dependencies**: 
  - `oxc` - JavaScript/TypeScript parser and transformer
  - `wit-bindgen` - WebAssembly Interface Types bindings
- **Output**: WebAssembly module for runtime code injection

## Firefox/Gecko Integration (`gecko/`)

```
gecko/
├── config/                # Firefox build configuration
├── branding/              # Browser branding assets
│   ├── floorp-official/   # Official Floorp branding
│   ├── floorp-daylight/   # Daylight build branding
│   └── noraneko-unofficial/ # Noraneko branding
└── pre-build/             # Pre-build configuration
```

## Type Definitions (`@types/`)

```
@types/
└── gecko/                 # Firefox/Gecko TypeScript definitions
    └── dom/               # DOM API type definitions
```

## Configuration Files

### Deno Configuration (`deno.json`)
- **Workspace**: Manages multiple packages and applications
- **Tasks**: Defines build, development, and deployment tasks
- **Imports**: Maps package imports and dependencies
- **Key Tasks**:
  - `dev` - Start development server
  - `build` - Build for production
  - `clobber` - Clean build artifacts

### Package Management
- **Node.js** (`package.json`): Frontend dependencies and tooling
- **Rust** (`Cargo.toml`): Rust workspace and crates
- **Deno** (`deno.json`): Deno modules and workspace configuration

## Development Workflow

### 1. Environment Setup
```bash
# Install Deno
curl -fsSL https://deno.land/install.sh | sh

# Clone and setup
git clone <repository>
deno install
```

### 2. Development Mode
```bash
# Start development server with hot reload
deno task dev
```

### 3. Build Process
1. **Binary Download**: Fetches Firefox binary from GitHub releases
2. **Patch Application**: Applies custom patches to Firefox
3. **Code Compilation**: Builds TypeScript/SolidJS applications
4. **Asset Processing**: Processes CSS, images, and other assets
5. **Injection**: Injects custom code into Firefox binary
6. **Launch**: Starts modified Firefox with custom profile

### 4. Production Build
```bash
# Build for production
deno task build
```

## Key Technologies & Frameworks

### Frontend Stack
- **SolidJS**: Reactive UI framework for applications
- **TypeScript**: Type-safe JavaScript development
- **Tailwind CSS**: Utility-first CSS framework
- **Vite**: Build tool and development server
- **PostCSS**: CSS processing and optimization

### Backend/Build Stack
- **Deno**: Runtime for build scripts and development tools
- **Rust**: Performance-critical components and WebAssembly
- **Node.js**: Package management and some tooling
- **Mozilla Build System**: Integration with Firefox build process

### Browser Integration
- **XUL**: Firefox's XML-based UI language
- **WebExtensions**: Browser extension APIs
- **XPCOM**: Cross-platform component object model
- **Gecko**: Firefox's browser engine

## Architecture Patterns

### 1. Modular Application Architecture
- Applications are self-contained with their own build configurations
- Shared packages provide common functionality
- Clear separation between UI applications and core browser modifications

### 2. Injection-Based Customization
- Custom functionality is injected into Firefox rather than forking
- Maintains compatibility with Firefox updates
- Allows for hot-reload during development

### 3. Multi-Language Build System
- Deno for build orchestration and scripting
- TypeScript for application logic
- Rust for performance-critical operations
- Integration with Mozilla's build system

### 4. Theme System Architecture
- Pluggable theme system with multiple theme options
- CSS-based customization with preprocessing
- Runtime theme switching capabilities

## Security Considerations

### Code Injection Safety
- All injected code goes through validation
- WebAssembly sandboxing for Rust components
- Careful handling of user scripts and customizations

### Update Mechanism
- Secure download and verification of Firefox binaries
- Patch integrity checking
- Staged rollout of updates

## Performance Optimizations

### Build Performance
- Parallel processing of applications
- Incremental builds during development
- Efficient asset bundling and optimization

### Runtime Performance
- WebAssembly for performance-critical operations
- Lazy loading of UI components
- Optimized CSS and JavaScript delivery

## Testing Strategy

### Unit Testing
- Vitest for JavaScript/TypeScript testing
- Karma/Jasmine for browser-specific tests
- Rust unit tests for native components

### Integration Testing
- End-to-end testing with browser automation
- Cross-platform testing on Windows, macOS, and Linux
- Theme and customization testing

## Deployment & Distribution

### Platform Support
- **Windows**: EXE installer, Winget, Scoop
- **macOS**: DMG with Apple notarization, Homebrew
- **Linux**: PPA, Flatpak, tarball, AUR packages

### Build Artifacts
- Platform-specific binaries
- Installer packages
- Update packages for auto-update system

## Contributing Guidelines

### Development Setup
1. Install Deno runtime
2. Clone repository and run `deno install`
3. Run `deno task dev` for development mode
4. Browser launches with hot-reload enabled

### Code Organization
- Applications in `src/apps/` for UI components
- Packages in `src/packages/` for reusable libraries
- Scripts in `scripts/` for build and automation
- Rust components in `crates/` for performance-critical code

### Build System Integration
- All builds go through the main `build.ts` script
- Development mode provides hot-reload
- Production builds create optimized, distributable packages

This architecture enables Floorp to maintain compatibility with Firefox while providing extensive customization capabilities and a modern development experience.