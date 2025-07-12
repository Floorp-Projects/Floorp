# Floorp Browser - Architecture Overview

## Project Overview

Floorp is an independent browser based on Mozilla Firefox, designed to maintain an open, private, and sustainable web. It maintains Firefox's stability and security while providing unique features and an improved user experience.

## Design Philosophy

### 1. Firefox Compatibility Maintenance

- Uses Firefox's core engine (Gecko) as-is
- Architecture that can follow Firefox updates
- Compatibility with existing Firefox extensions

### 2. Modular Design

- Independent applications for each feature
- Reusable package system
- Pluggable theme system

### 3. Modern Development Experience

- Type safety with TypeScript
- High-performance UI with SolidJS
- Hot reload-enabled development environment

### 4. Multi-Platform Support

- Unified experience across Windows, macOS, and Linux
- Platform-specific optimizations

## Overall Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Floorp Browser                           │
├─────────────────────────────────────────────────────────────┤
│  UI Applications (SolidJS + TypeScript)                     │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐   │
│  │  Main   │ │Settings │ │ New Tab │ │ Other Apps      │   │
│  │  App    │ │  App    │ │   App   │ │ (Notes, etc.)   │   │
│  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Shared Packages & Libraries                                 │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Solid-XUL   │ │ Skin        │ │ User Script         │   │
│  │             │ │ System      │ │ Runner              │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Firefox/Gecko Engine                                        │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Gecko Core  │ │ XUL/XPCOM   │ │ WebExtensions API   │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Major Components

### 1. UI Application Layer

**Technology**: SolidJS + TypeScript + Tailwind CSS

- **Main Application**: Core browser functionality and UI
- **Settings Application**: User settings and customization
- **New Tab**: Customizable start page
- **Others**: Notes, welcome screen, modal dialogs

### 2. Shared Package Layer

**Technology**: TypeScript + Modular Design

- **Solid-XUL**: Integration of SolidJS and Firefox XUL
- **Skin System**: Theme and styling management
- **User Script Runner**: Custom script execution
- **Test Utilities**: Testing support tools

### 3. Firefox/Gecko Integration Layer

**Technology**: C++ + JavaScript + XUL

- **Gecko Engine**: Firefox's browser engine
- **XUL/XPCOM**: Firefox's UI framework
- **WebExtensions**: Browser extension API

## Data Flow

### 1. Development Data Flow

```
Developer Code Changes
    ↓
Deno Build System
    ↓
TypeScript Compilation + SolidJS Transformation
    ↓
Code Injection into Firefox Binary
    ↓
Launch of Customized Firefox
```

### 2. Runtime Data Flow

```
User Interaction
    ↓
SolidJS Application
    ↓
Solid-XUL Bridge
    ↓
Firefox XUL System
    ↓
Gecko Engine
    ↓
Web Page Display & Interaction
```
