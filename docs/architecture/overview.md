# Floorp Browser - Architecture Overview

## Project Overview

Floorp is an independent browser based on Mozilla Firefox, designed to maintain an open, private, and sustainable web. It provides unique features and improved user experience while maintaining Firefox's stability and security.

## Design Philosophy

### 1. Maintaining Firefox Compatibility
- Uses Firefox's core engine (Gecko) as-is
- Architecture that can follow Firefox updates
- Compatibility with existing Firefox extensions

### 2. Modular Design
- Independent applications for each feature
- Reusable package system
- Pluggable theme system

### 3. Modern Development Experience
- Type safety through TypeScript
- High-performance UI with SolidJS
- Development environment with hot reload support

### 4. Multi-platform Support
- Unified experience across Windows, macOS, and Linux
- Platform-specific optimizations

## Overall Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Floorp Browser                           │
├─────────────────────────────────────────────────────────────┤
│  UI Applications (SolidJS + TypeScript)                    │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐   │
│  │  Main   │ │Settings │ │ NewTab  │ │ Other Apps      │   │
│  │   App   │ │   App   │ │   App   │ │ (Notes, etc.)   │   │
│  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Shared Packages & Libraries                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Solid-XUL   │ │ Skin System │ │ User Script Runner  │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Rust Components (WebAssembly)                             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │              Nora-Inject (Code Injection)              │ │
│  └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  Firefox/Gecko Engine                                       │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Gecko Core  │ │ XUL/XPCOM   │ │ WebExtensions API   │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Major Components

### 1. UI Application Layer
**Technology**: SolidJS + TypeScript + Tailwind CSS

- **Main Application**: Primary browser functionality and UI
- **Settings Application**: User settings and customization
- **New Tab**: Customizable start page
- **Others**: Notes, welcome screen, modal dialogs

### 2. Shared Package Layer
**Technology**: TypeScript + Modular Design

- **Solid-XUL**: Integration between SolidJS and Firefox XUL
- **Skin System**: Theme and styling management
- **User Script Runner**: Custom script execution
- **Test Utilities**: Testing support tools

### 3. Rust Component Layer
**Technology**: Rust + WebAssembly

- **Nora-Inject**: High-performance code injection system
- **Performance Optimization**: Acceleration of critical processes
- **Security**: Safe code execution environment

### 4. Firefox/Gecko Integration Layer
**Technology**: C++ + JavaScript + XUL

- **Gecko Engine**: Firefox's browser engine
- **XUL/XPCOM**: Firefox's UI framework
- **WebExtensions**: Browser extension APIs

## Data Flow

### 1. Development Data Flow
```
Developer Code Changes
    ↓
Deno Build System
    ↓
TypeScript Compilation + SolidJS Transformation
    ↓
Rust Component Build (WebAssembly)
    ↓
Code Injection into Firefox Binary
    ↓
Launch Customized Firefox
```

### 2. Runtime Data Flow
```
User Operations
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

## Technology Choice Rationale

### SolidJS Adoption
- **High Performance**: Efficient updates without virtual DOM
- **Small Bundle Size**: Lightweight for browser embedding
- **React-like API**: Familiar to developers

### Rust + WebAssembly Adoption
- **Performance**: Native-level execution speed
- **Safety**: Memory safety and thread safety
- **Sandbox**: Isolated execution through WebAssembly

### Deno Adoption
- **Modern Runtime**: Native ES modules and TypeScript support
- **Security**: Secure execution environment by default
- **Toolchain Integration**: Integrated formatter, linter, and test runner

## Security Architecture

### 1. Code Injection Safety
- Validation and sanitization of injected code
- Isolation through WebAssembly sandbox
- Principle of least privilege

### 2. Update Mechanism
- Digital signature verification of updates
- Staged rollout
- Rollback functionality

### 3. User Data Protection
- Inherits Firefox's existing security features
- Additional privacy protection features
- Data encryption and secure storage

## Performance Optimization

### 1. Build-time Optimization
- Tree shaking to remove unnecessary code
- Code splitting for efficient loading
- Static asset optimization

### 2. Runtime Optimization
- Efficient DOM updates with SolidJS
- High-speed processing with WebAssembly
- Lazy loading and caching strategies

### 3. Memory Management
- Memory safety through Rust's ownership system
- Garbage collection optimization
- Proper resource cleanup

## Extensibility and Maintainability

### 1. Modular Design
- Independent packages for each feature
- Clear dependency management
- Interface-based design

### 2. Testing Strategy
- Unit tests: Individual component testing
- Integration tests: Inter-component interaction testing
- E2E tests: User scenario testing

### 3. Documentation
- In-code documentation
- API reference
- Architecture documentation

## Future Expansion Plans

### 1. New Feature Addition
- Plugin system expansion
- AI feature integration
- Cross-device synchronization

### 2. Performance Improvements
- Migration of more processing to Rust/WebAssembly
- GPU acceleration utilization
- Network optimization

### 3. Platform Support
- Mobile version consideration
- Embedded system support
- Cloud integration features

This architecture enables Floorp to provide a modern and extensible browser experience while maintaining Firefox's stability and security.