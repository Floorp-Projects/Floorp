# Directory Structure - Detailed Guide

## Root Directory Structure

```
floorp/
├── .git/                    # Git repository metadata
├── .github/                 # GitHub Actions workflows and templates
├── .vscode/                 # VSCode workspace settings
├── .claude/                 # Claude AI assistant settings
├── @types/                  # TypeScript type definitions
├── src/                     # Main source code directory
├── scripts/                 # Build and automation scripts
├── gecko/                   # Firefox/Gecko integration files
├── _dist/                   # Build output directory (generated)
├── build.ts                 # Main build script
├── deno.json               # Deno configuration and tasks
├── package.json            # Node.js dependencies
├── tsconfig.json           # TypeScript configuration
├── moz.build               # Mozilla build system integration
├── biome.json              # Biome configuration (formatter/linter)
├── LICENSE                 # License file
├── README.md               # Project documentation
└── TODO.md                 # TODO list
```

## src/ Directory - Main Source Code

### Overall Structure

```
src/
├── apps/                   # UI applications
├── packages/               # Reusable packages
├── installers/             # Installers
└── .oxlintrc.json         # Oxlint configuration
```

### src/apps/ - Applications

```
src/apps/
├── main/                   # Main browser application
│   ├── core/              # Core functionality
│   │   ├── index.ts       # Entry point
│   │   ├── modules.ts     # Module management
│   │   ├── modules-hooks.ts # Module hooks
│   │   ├── common/        # Common utilities
│   │   ├── static/        # Static assets
│   │   ├── test/          # Test files
│   │   └── utils/         # Utility functions
│   ├── i18n/              # Internationalization files
│   ├── docs/              # Documentation
│   ├── about/             # About page
│   ├── @types/            # Type definitions
│   ├── package.json       # Dependencies
│   ├── vite.config.ts     # Vite configuration
│   ├── tsconfig.json      # TypeScript configuration
│   ├── tailwind.config.js # Tailwind CSS configuration
│   └── deno.json          # Deno configuration
│
├── settings/              # Settings application
│   ├── src/               # Source code
│   │   ├── components/    # UI components
│   │   ├── pages/         # Page components
│   │   ├── utils/         # Utilities
│   │   └── main.tsx       # Entry point
│   ├── package.json       # Dependencies
│   ├── vite.config.ts     # Vite configuration
│   ├── components.json    # shadcn/ui configuration
│   └── index.html         # HTML template
│
├── newtab/                # New tab page
│   ├── src/               # Source code
│   │   ├── components/    # UI components
│   │   ├── stores/        # State management
│   │   └── main.tsx       # Entry point
│   ├── package.json       # Dependencies
│   ├── vite.config.ts     # Vite configuration
│   └── index.html         # HTML template
│
├── notes/                 # Notes functionality
├── welcome/               # Welcome screen
├── startup/               # Startup process
├── modal-child/           # Modal dialogs
├── os/                    # OS-specific functionality
├── i18n-supports/         # Internationalization support
├── modules/               # Shared modules
├── common/                # Common components
└── README.md              # Application overview
```

### src/packages/ - Packages

```
src/packages/
├── solid-xul/             # SolidJS + XUL integration
│   ├── index.ts           # Main API
│   ├── jsx-runtime.ts     # JSX runtime
│   ├── package.json       # Package configuration
│   └── tsconfig.json      # TypeScript configuration
│
├── skin/                  # Skin system
│   ├── fluerial/          # Fluerial theme
│   ├── lepton/            # Lepton theme
│   ├── noraneko/          # Noraneko theme
│   └── package.json       # Package configuration
│
├── user-js-runner/        # User script execution
│   ├── src/               # Source code
│   ├── package.json       # Package configuration
│   └── README.md          # Documentation
│
└── vitest-noraneko/       # Test utilities
    ├── src/               # Source code
    ├── package.json       # Package configuration
    └── README.md          # Documentation
```

### src/installers/ - Installers

```
src/installers/
└── stub-win64-installer/  # Windows 64-bit installer
    ├── src/               # Tauri application
    ├── src-tauri/         # Tauri configuration
    ├── package.json       # Dependencies
    └── tauri.conf.json    # Tauri configuration
```

## scripts/ - Build and Automation Scripts

```
scripts/
├── inject/                # Code injection related
│   ├── manifest.ts        # Manifest injection
│   ├── xhtml.ts          # XHTML template injection
│   ├── mixin-loader.ts   # Mixin system loader
│   ├── mixins/           # UI mixins
│   │   ├── browser/      # Browser UI mixins
│   │   ├── preferences/  # Settings UI mixins
│   │   └── shared/       # Shared mixins
│   ├── shared/           # Shared utilities
│   ├── wasm/             # WebAssembly modules
│   └── tsconfig.json     # TypeScript configuration
│
├── launchDev/            # Development server
│   ├── child-build.ts    # Child process build
│   ├── child-dev.ts      # Development server
│   ├── child-browser.ts  # Browser launch
│   ├── savePrefs.ts      # Settings save
│   └── writeVersion.ts   # Version write
│
├── update/               # Update related
│   ├── buildid2.ts       # Build ID management
│   ├── version.ts        # Version management
│   └── xml.ts            # XML processing
│
├── cssUpdate/            # CSS updates
│   ├── processors/       # CSS processors
│   └── watchers/         # File watchers
│
├── git-patches/          # Git patch management
│   ├── git-patches-manager.ts # Main patch manager
│   └── patches/          # Patch files
│       ├── firefox/      # Firefox patches
│       └── gecko/        # Gecko patches
│
├── i18n/                 # Internationalization
│   ├── extractors/       # String extractors
│   ├── translators/      # Translation processing
│   └── validators/       # Translation validation
│
└── .oxlintrc.json       # Oxlint configuration
```

## gecko/ - Firefox/Gecko Integration

```
gecko/
├── config/               # Firefox build configuration
│   ├── moz.configure     # Mozilla configuration
│   ├── README.md         # Configuration documentation
│   └── .gitignore        # Git ignore settings
│
├── branding/             # Branding
│   ├── floorp-official/  # Official Floorp branding
│   │   ├── icons/        # Icon files
│   │   ├── locales/      # Localization
│   │   └── configure.sh  # Configuration script
│   ├── floorp-daylight/  # Daylight build branding
│   └── noraneko-unofficial/ # Unofficial Noraneko branding
│
└── pre-build/            # Pre-build configuration
    ├── patches/          # Pre-build patches
    ├── configs/          # Configuration files
    └── scripts/          # Pre-build scripts
```

## @types/ - TypeScript Type Definitions

```
@types/
└── gecko/                # Firefox/Gecko type definitions
    └── dom/              # DOM API type definitions
        ├── index.d.ts    # Main type definitions
        ├── xul.d.ts      # XUL type definitions
        ├── xpcom.d.ts    # XPCOM type definitions
        └── webext.d.ts   # WebExtensions type definitions
```

## \_dist/ - Build Output (Generated)

```
_dist/
├── bin/                  # Binary files
│   └── floorp/           # Floorp binary
├── noraneko/             # Built applications
│   ├── content/          # Content files
│   ├── startup/          # Startup files
│   ├── resource/         # Resource files
│   ├── settings/         # Settings app
│   ├── newtab/           # New tab app
│   ├── welcome/          # Welcome app
│   ├── notes/            # Notes app
│   ├── modal-child/      # Modal dialogs
│   └── os/               # OS-specific files
├── profile/              # Development profile
└── buildid2              # Build ID
```

## Configuration File Details

### deno.json - Deno Configuration

```json
{
  "workspace": ["./src/packages/*", "./src/apps/*"],
  "imports": {
    "@std/encoding": "jsr:@std/encoding@^1.0.7",
    "@std/fs": "jsr:@std/fs@^1.0.11"
  },
  "tasks": {
    "dev": "deno run -A build.ts --run",
    "build": "deno run -A build.ts"
  }
}
```

### package.json - Node.js Dependencies

- Frontend development tools
- Build tools
- UI libraries

### moz.build - Mozilla Build System Integration

```python
DIRS += [
    "_dist/noraneko/content",
    "_dist/noraneko/startup",
    # ... other directories
]
```

## File Naming Conventions

### TypeScript/JavaScript

- **Components**: PascalCase (`MainComponent.tsx`)
- **Utilities**: camelCase (`utilityFunction.ts`)
- **Configuration**: kebab-case (`vite.config.ts`)

### CSS/SCSS

- **Files**: kebab-case (`main-styles.css`)
- **Classes**: BEM notation or Tailwind

### Assets

- **Images**: kebab-case (`app-icon.png`)
- **Fonts**: kebab-case (`custom-font.woff2`)

## Directory Roles and Responsibilities

### Development Related

- `src/apps/`: UI application implementation
- `src/packages/`: Reusable libraries
- `scripts/`: Build and development support tools

### Build Related

- `_dist/`: Build artifacts (temporary)
- `build.ts`: Build orchestration
- Each app's `vite.config.ts`: Individual build configuration

### Integration Related

- `gecko/`: Firefox integration configuration
- `@types/`: Type safety assurance

This structure allows Floorp to manage complex browser applications in an organized manner, providing developers with an efficient working environment.
