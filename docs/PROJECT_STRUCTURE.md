# Noraneko Project Structure Guide

## 🎯 Proposed Developer-Friendly Structure

```
noraneko/
├── docs/                           # 📚 Documentation
│   ├── PROJECT_STRUCTURE.md        # This file
│   ├── DEVELOPMENT_GUIDE.md        # Step-by-step development guide
│   ├── API_REFERENCE.md            # API documentation
│   └── TROUBLESHOOTING.md          # Common issues and solutions
│
├── src/                            # 🛠️ Source Code
│   ├── core/                       # Core browser functionality
│   │   ├── glue/                   # 🔗 Browser integration glue code (STABLE)
│   │   │   ├── loader-features/    # Feature loading system
│   │   │   ├── loader-modules/     # Module loading system  
│   │   │   └── startup/            # Browser startup integration
│   │   ├── modules/                # System modules (.sys.mts files)
│   │   └── actors/                 # Browser actors
│   │
│   ├── features/                   # 🚀 Browser features (ACTIVE DEVELOPMENT)
│   │   ├── workspaces/             # Workspace functionality
│   │   ├── split-view/             # Split view feature
│   │   └── pwa-manager/            # Progressive Web App management
│   │
│   ├── ui/                         # User Interface
│   │   ├── chrome/                 # Browser chrome (toolbars, menus)
│   │   ├── content/                # Content area components
│   │   ├── settings/               # Settings pages
│   │   └── about-pages/            # about:* pages
│   │
│   ├── themes/                     # Visual themes
│   │   ├── default/                # Default Noraneko theme
│   │   ├── lepton/                 # Lepton theme integration
│   │   └── fluerial/               # Fluerial theme
│   │
│   └── shared/                     # Shared utilities and libraries
│       ├── utils/                  # Common utilities
│       ├── types/                  # TypeScript type definitions
│       └── constants/              # Application constants
│
├── tools/                          # 🔧 Development Tools
│   ├── build/                      # Build orchestration
│   │   ├── phases/                 # Build phases (before/after mozbuild)
│   │   └── tasks/                  # Individual build tasks
│   └── dev/                        # Development utilities
│
├── config/                         # ⚙️ Configuration
│   ├── vite/                       # Vite configurations
│   ├── deno/                       # Deno configurations
│   └── build/                      # Build configurations
│
├── dist/                           # 📦 Build Output
│   ├── bin/                        # Binary files
│   ├── profile/                    # Profile data
│   └── temp/                       # Temporary files
│
├── tests/                          # 🧪 Tests
│   ├── unit/                       # Unit tests
│   ├── integration/                # Integration tests
│   └── e2e/                        # End-to-end tests
│
└── assets/                         # 📎 Static Assets
    ├── icons/                      # Application icons
    ├── images/                     # Images and graphics
    └── locales/                    # Internationalization files
```

## 🚀 Benefits of This Structure

### 1. **Clear Separation of Concerns**
- `src/` contains all source code
- `tools/` contains development utilities
- `config/` contains all configuration files
- `docs/` contains comprehensive documentation

### 2. **Intuitive Navigation**
- Features are grouped logically under `src/features/`
- UI components are organized under `src/ui/`
- Themes are centralized under `src/themes/`

### 3. **Development Efficiency**
- Related files are co-located
- Clear naming conventions
- Reduced cognitive load for new developers

### 4. **Scalability**
- Easy to add new features
- Modular structure supports plugin architecture
- Clear boundaries between components

## 📋 Migration Strategy

### Phase 1: Documentation and Planning
1. Create comprehensive documentation
2. Set up development guides
3. Establish coding standards

### Phase 2: Gradual Restructuring
1. Move and consolidate related files
2. Update import paths
3. Update build configurations

### Phase 3: Tooling Updates
1. Update build scripts
2. Update development workflows
3. Update CI/CD pipelines

## 🎯 Immediate Actions

### 1. Separate Build System from Application Code

**Current Problem**: `apps/system/` mixes build scripts with browser glue code, causing confusion for feature developers.

**Proposed Structure**:
```
├── src/                            # 🛠️ Source Code
│   ├── core/                       # Core browser functionality  
│   │   ├── glue/                   # 🔗 Browser integration glue code (STABLE)
│   │   │   ├── loader-features/    # Feature loading system
│   │   │   ├── loader-modules/     # Module loading system  
│   │   │   └── startup/            # Browser startup integration
│   │   ├── modules/                # System modules (.sys.mts files)
│   │   └── actors/                 # Browser actors
│   │
│   ├── features/                   # 🚀 Browser features (ACTIVE DEVELOPMENT)
│   │   ├── workspaces/             # Workspace functionality
│   │   ├── split-view/             # Split view feature
│   │   └── pwa-manager/            # Progressive Web App management
│   │
├── tools/                          # 🔧 Development Tools
│   ├── build/                      # Build orchestration
│   │   ├── phases/                 # Build phases (before/after mozbuild)
│   │   └── tasks/                  # Individual build tasks
│   └── dev/                        # Development utilities
```

**Benefits**:
- **Clear Separation**: Glue code in `src/core/glue/` (stable, rarely changed)
- **Feature Focus**: Developers work in `src/features/` without distraction
- **Build Configs Colocated**: Build scripts stay with their respective glue code
- **Reduced Complexity**: Clear boundaries between stable glue and active development

### 2. Migration Steps

1. **Phase 1**: Move glue code
   ```bash
   # Move browser integration code to core
   mv apps/system/loader-features src/core/glue/
   mv apps/system/loader-modules src/core/glue/
   mv apps/system/startup src/core/glue/
   ```

2. **Phase 2**: Keep build configurations colocated
   ```bash
   # Build configurations remain with their respective glue code
   # This keeps related functionality together and simplifies maintenance
   # Example structure:
   # src/core/glue/loader-features/vite.config.ts
   # src/core/glue/loader-modules/tsdown.config.ts
   # src/core/glue/startup/tsdown.config.ts
   ```

3. **Phase 3**: Update import paths and build scripts
   - Update `scripts/build.ts` to reference new paths
   - Update symlink creation in build system
   - Update vite configurations to work with colocated build configs

**Implementation Priority**:
1. ✅ **Create Developer Onboarding Guide** 
2. 🔄 **Separate Glue Code from Build Scripts** (This proposal)
3. **Standardize Configuration Files**
4. **Improve Build Script Organization** 
5. **Add Component Documentation**
6. **Set Up Development Templates**

## ✅ Migration Progress

### ✅ **Phase 1 Completed: Tools Directory Restructuring** 

**Status**: **COMPLETED** ✅

The `scripts/` directory has been successfully migrated to the new `tools/` structure:

```
tools/                          # 🔧 Development Tools
├── build/                      # Build orchestration
│   ├── phases/                 # Build phases (before/after mozbuild)
│   │   ├── pre-build.ts        # Pre-Mozilla build tasks
│   │   └── post-build.ts       # Post-Mozilla build tasks
│   ├── tasks/                  # Individual build tasks
│   │   ├── inject/             # Code injection tasks
│   │   ├── git-patches/        # Git patch management
│   │   └── update/             # Update-related tasks
│   ├── index.ts                # Main build orchestrator
│   ├── defines.ts              # Path and constant definitions
│   ├── logger.ts               # Logging utilities
│   └── utils.ts                # Common utilities
│
└── dev/                        # Development utilities
    ├── launchDev/              # Development launch tools
    ├── prepareDev/             # Development preparation
    ├── cssUpdate/              # CSS update tools
    └── index.ts                # Development tools entry point
```

**Benefits Achieved**:
- ✅ **Clear Separation**: Build tools vs development tools
- ✅ **Modular Design**: Phase-based build system with reusable tasks
- ✅ **Improved CLI**: Intuitive command-line interface with help messages
- ✅ **Better Error Handling**: Consistent error reporting and logging
- ✅ **Path Normalization**: All paths relative to project root from tools directory

**New Usage**:
```bash
# Development build
deno run -A build.ts --dev

# Production build  
deno run -A build.ts --production

# Development tools
deno run -A tools/dev/index.ts start
deno run -A tools/dev/index.ts clean
```

### 🔄 **Next Phase: Source Code Restructuring**

**Target**: Move application source code to the proposed `src/` structure:
- `apps/system/` → `src/core/glue/`
- Feature code → `src/features/`
- UI components → `src/ui/`
- Themes → `src/themes/`

**Benefits Expected**:
- **Enhanced Clarity**: Distinct folders for glue code, features, UI, and themes
- **Improved Collaboration**: Frontend and backend developers can work independently
- **Easier Navigation**: Logical grouping of related files
- **Streamlined Development**: Focused areas for feature development and bug fixing
