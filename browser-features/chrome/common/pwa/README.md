# PWA

PWA stands for Progressive Web App, which is a technology that allows web pages available in modern browsers to be used like applications.

## What is PWA on Floorp?

Floorp realizes PWA by refactoring Firefox's code, utilizing the SSB (Site Specific Browser) feature that Firefox provided.

For example, ManifestProcesser.ts is written with reference to Firefox code, and functions with the same name exist.

## Architecture Overview

The PWA feature consists mainly of two parts:

1. **Frontend part** (`src/apps/main/core/common/pwa/`)

   - UI elements (SsbPageAction, SsbPanelView)
   - PWA window management
   - High-level service API

2. **Backend part** (`src/apps/modules/src/modules/pwa/`)
   - Command line integration
   - Data storage
   - OS integration features

## Main Components

- **PwaService**: Central service class. Integrates other components and provides high-level API
- **SiteSpecificBrowserManager**: Responsible for PWA installation and management
- **SsbRunner**: Handles launching PWA windows
- **DataManager**: Manages information of installed PWAs
- **ManifestProcesser**: Extracts and processes PWA information from web manifests

## OS Integration

Currently, the following integration features are available on the Windows platform:

- Pinning to the taskbar
- Launching in dedicated windows
- Icon integration

On macOS, launcher bundles are created in `~/Applications/Floorp Apps/`.
Each bundle contains an executable launcher, `Info.plist`, and a 512px ICNS icon.
The launcher passes the installing profile and SSB ID to Floorp. Bundle identity
includes the profile, so copied SSB IDs in different profiles do not collide.
Opening an existing PWA from Floorp also creates or repairs its launcher.
Renaming or uninstalling a PWA updates or removes its owned bundle.

This is launcher integration: it does not automatically pin apps to the Dock
or provide a separate native process/Dock identity for each running PWA.
On macOS, verify Finder launch with Floorp both running and closed, manual Dock
pinning, icon display, rename/uninstall, and two profiles before release.
File-generation tests on other platforms do not verify Launch Services behavior.

PWA regression tests are colocated under `test/` and under
`browser-features/modules/modules/pwa/supports/test/`. They cover toolbar
preferences (including existing configurations and live changes), legacy SSB
data migration, launcher metadata, profile isolation, bundle repair/removal,
and fallback icon encoding. Run with `deno task test --near <directory>` in
an isolated test profile.

## Directory Structure

```plaintext
src/apps/main/core/common/pwa/
├── config.ts             # Configuration options
├── dataStore.ts          # Data persistence
├── default-pref.ts       # Default settings
├── iconProcesser.ts      # Icon processing
├── imageTools.ts         # Image processing utilities
├── index.ts              # Main entry point
├── manifestProcesser.ts  # Manifest processing
├── pwa-window.tsx        # PWA window UI
├── pwaService.ts         # Central service
├── SsbPageAction.tsx     # URL bar PWA action
├── SsbPanelView.tsx      # PWA management panel
├── ssbManager.ts         # SSB management
├── ssbRunner.ts          # SSB execution
└── type.ts               # Type definitions

src/apps/modules/src/modules/pwa/
├── DataStore.sys.mts           # System-level data store
├── SsbCommandLineHandler.sys.mts # Command line integration
├── ImageTools.sys.mts          # Image processing utilities
├── supports/                   # OS-specific support features
└── type.ts                     # Type definitions
```

## Developer Information

To extend the PWA functionality, you mainly need to understand the following files:

- `pwaService.ts`: Adding/changing service APIs
- `ssbManager.ts`: Extending installation and management features
- `manifestProcesser.ts`: Customizing manifest processing
- `SsbPageAction.tsx`/`SsbPanelView.tsx`: Customizing UI elements

To add support for a new platform, implement it in the `supports/` directory.
