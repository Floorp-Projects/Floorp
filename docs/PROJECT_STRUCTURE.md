# Noraneko Project Structure Guide

## 🎯 Current Project Structure

```text
noraneko/
├── docs/                           # 📚 Documentation
│   ├── PROJECT_STRUCTURE.md        # This file
│   ├── DEVELOPMENT_GUIDE.md        # Step-by-step development guide
│   ├── BUILD_SYSTEM_IMPROVEMENTS.md # Build system documentation
│   └── README.md                   # Documentation index
│
├── src/                            # 🛠️ Source Code
│   ├── core/                       # Core browser functionality
│   │   ├── glue/                   # 🔗 Browser integration glue code (STABLE)
│   │   └── modules/                # System modules (.sys.mts files)
│   │
│   ├── features/                   # 🚀 Browser features (ACTIVE DEVELOPMENT)
│   │   └── chrome/                 # Chrome-level browser features
│   │
│   ├── ui/                         # User Interface
│   │   ├── about-pages/            # about:* pages
│   │   └── settings/               # Settings pages
│   │
│   ├── themes/                     # Visual themes
│   │   ├── fluerial/               # Fluerial theme
│   │   ├── lepton/                 # Lepton theme integration
│   │   └── noraneko/               # Default Noraneko theme
│   │
│   └── shared/                     # Shared utilities and libraries
│       ├── common/                 # Common utilities
│       ├── constants/              # Application constants
│       ├── types/                  # TypeScript type definitions
│       └── utils/                  # Utility functions
│
├── tools/                          # 🔧 Development Tools
│   ├── build/                      # Build orchestration
│   │   ├── phases/                 # Build phases (before/after mozbuild)
│   │   ├── tasks/                  # Individual build tasks
│   │   └── index.ts                # Main build entry point
│   └── dev/                        # Development utilities
│       ├── launchDev/              # Development launch scripts
│       ├── prepareDev/             # Development preparation
│       └── cssUpdate/              # CSS update tools
│
├── libs/                           # 📚 Libraries and dependencies
│   ├── @types/                     # Type definitions
│   ├── crates/                     # Rust crates
│   ├── shared/                     # Shared libraries
│   ├── skin/                       # UI skin libraries
│   ├── solid-xul/                  # Solid.js XUL integration
│   ├── user-js-runner/             # User script runner
│   └── vitest-noraneko/            # Testing framework
│
├── config/                         # ⚙️ Configuration
├── i18n/                           # 🌐 Internationalization
├── static/                         # 📦 Static assets
├── tests/                          # 🧪 Test files
├── assets/                         # 📎 Application assets
├── build/                          # 🏗️ Build configurations
└── _dist/                          # 📦 Build output (generated)
    ├── bin/                        # Binary files
    ├── profile/                    # Profile data
    └── temp/                       # Temporary files
```

## 🚀 Key Architecture Principles

### 1. **Clear Separation of Concerns**

- **`src/core/`**: Stable browser integration code
- **`src/features/`**: Active feature development
- **`src/ui/`**: User interface components
- **`src/themes/`**: Visual theming
- **`tools/`**: Development and build tools

### 2. **Modular Build System**

- **Phase-based builds**: Pre and post Mozilla build phases
- **Task-oriented**: Individual build tasks for specific operations
- **Tool separation**: Build tools separate from development tools

### 3. **Feature-Driven Development**

- Features are self-contained in `src/features/`
- Clear boundaries between core and features
- Easy to add, remove, or modify features

## 📋 Directory Responsibilities

### Source Code (`src/`)

| Directory          | Purpose             | Stability | Examples                |
| ------------------ | ------------------- | --------- | ----------------------- |
| `core/glue/`       | Browser integration | 🟢 Stable | Loader systems, startup |
| `core/modules/`    | System modules      | 🟢 Stable | .sys.mts files          |
| `features/chrome/` | Browser features    | 🟡 Active | Custom features         |
| `ui/`              | User interfaces     | 🟡 Active | Settings, about pages   |
| `themes/`          | Visual themes       | 🟡 Active | Fluerial, Lepton        |
| `shared/`          | Common utilities    | 🟢 Stable | Types, constants        |

### Tools (`tools/`)

| Directory | Purpose               | Usage                |
| --------- | --------------------- | -------------------- |
| `build/`  | Build orchestration   | Production builds    |
| `dev/`    | Development utilities | Development workflow |

### Libraries (`libs/`)

| Directory    | Purpose          | Technology |
| ------------ | ---------------- | ---------- |
| `@types/`    | Type definitions | TypeScript |
| `crates/`    | Rust libraries   | Rust       |
| `shared/`    | Shared libraries | TypeScript |
| `solid-xul/` | UI framework     | Solid.js   |

## 🎯 Development Workflow

### 1. **Feature Development**

```bash
# Work in features directory
cd src/features/chrome/

# Add new feature
mkdir my-new-feature
cd my-new-feature
```

### 2. **UI Development**

```bash
# Work in UI directory
cd src/ui/

# Add new UI component
mkdir my-component
cd my-component
```

### 3. **Theme Development**

```bash
# Work in themes directory
cd src/themes/

# Modify existing theme or create new one
cd noraneko/  # or fluerial/, lepton/
```

## 🔧 Build System Integration

### Current Structure Benefits

1. **Modular Build Process**: Each phase has specific responsibilities
2. **Clear Dependencies**: Build order is well-defined
3. **Development Tools**: Separate from production build
4. **Maintainable**: Easy to understand and modify

### Build Phases

1. **Pre-Mozilla Build**: Preparation and setup
2. **Mozilla Build**: Core Firefox build
3. **Post-Mozilla Build**: Noraneko integration

## 📚 Related Documentation

- [Development Guide](DEVELOPMENT_GUIDE.md) - Step-by-step development instructions
- [Build System Documentation](BUILD_SYSTEM_IMPROVEMENTS.md) - Build system details
- [Documentation Index](README.md) - All documentation links

## 🎯 Migration Status

### ✅ Completed

- Tools directory restructuring
- Build system modularization
- Documentation organization

### 🔄 In Progress

- Source code organization refinement
- Feature development guidelines
- Testing framework integration

### 📋 Future Plans

- Component documentation
- Development templates
- CI/CD pipeline improvements
