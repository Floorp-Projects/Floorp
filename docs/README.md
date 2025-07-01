# Floorp Browser Documentation

Welcome to the comprehensive documentation for Floorp Browser. This documentation is created for developers and contributors to the Floorp project.

## 📚 Documentation Structure

### 🏗️ Architecture
- [**Project Overview**](./architecture/overview.md) - Overall architecture and design philosophy of Floorp
- [**Directory Structure**](./architecture/directory-structure.md) - Detailed explanation of project directory organization
- [**Technology Stack**](./architecture/tech-stack.md) - Technologies and frameworks used
- [**Build System**](./architecture/build-system.md) - Build pipeline and processing flow

### 🔧 Development
- [**Development Environment Setup**](./development/setup.md) - How to set up the development environment
- [**Development Workflow**](./development/workflow.md) - Daily development workflow
- [**Debugging & Testing**](./development/debugging-testing.md) - Debugging and testing methods
- [**Coding Standards**](./development/coding-standards.md) - Code style and conventions

### 📱 Applications
- [**Main Application**](./applications/main-app.md) - Main browser functionality
- [**Settings Application**](./applications/settings.md) - Settings and preferences UI
- [**New Tab**](./applications/newtab.md) - New tab page
- [**Other Applications**](./applications/other-apps.md) - Notes, welcome screen, etc.

### 📦 Packages & Libraries
- [**Solid-XUL**](./packages/solid-xul.md) - SolidJS and XUL integration
- [**Skin System**](./packages/skin-system.md) - Theming and styling
- [**User Scripts**](./packages/user-scripts.md) - User script execution engine
- [**Shared Libraries**](./packages/shared-libraries.md) - Common libraries and utilities

### 🦀 Rust Components
- [**Nora-Inject**](./rust/nora-inject.md) - Rust-based code injection system
- [**WebAssembly Integration**](./rust/webassembly.md) - WASM module configuration and usage
- [**Performance Optimization**](./rust/performance.md) - Rust-powered optimizations

### 🔌 Firefox Integration
- [**Gecko Integration**](./firefox/gecko-integration.md) - Integration with Firefox engine
- [**Patch System**](./firefox/patch-system.md) - Firefox patch application
- [**XUL and WebExtensions**](./firefox/xul-webextensions.md) - UI extensions and browser APIs

### 🚀 Deployment
- [**Build and Release**](./deployment/build-release.md) - Production builds
- [**Installers**](./deployment/installers.md) - Platform-specific installers
- [**Distribution and Updates**](./deployment/distribution.md) - Distribution methods and auto-updates

### 🔐 Security
- [**Security Considerations**](./security/considerations.md) - Security design and measures
- [**Code Injection Safety**](./security/code-injection.md) - Safe code injection implementation
- [**Update Mechanism**](./security/update-mechanism.md) - Secure update system

### 🤝 Contributing
- [**Contribution Guide**](./contributing/guide.md) - How to contribute to the project
- [**Code Review**](./contributing/code-review.md) - Code review process
- [**Issues and PRs**](./contributing/issues-prs.md) - Issue and pull request management

## 🚀 Quick Start

1. **Development Environment Setup**
   ```bash
   # Install Deno
   curl -fsSL https://deno.land/install.sh | sh
   
   # Clone repository
   git clone <repository-url>
   cd floorp
   
   # Install dependencies
   deno install
   ```

2. **Start Development Server**
   ```bash
   # Start in development mode (hot reload enabled)
   deno task dev
   ```

3. **Build**
   ```bash
   # Build for production
   deno task build
   ```

## 📋 Required Knowledge

Recommended technical knowledge for participating in Floorp development:

- **TypeScript/JavaScript** - Application development
- **SolidJS** - Reactive UI framework
- **Rust** - Performance-critical components (optional)
- **Firefox/Gecko** - Basic browser engine knowledge
- **Deno** - Runtime and build tools

## 🆘 Support

If you need help:

- **Discord**: [Official Discord Server](https://discord.floorp.app)
- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and discussions

## 📄 License

This project is licensed under the [Mozilla Public License 2.0](../LICENSE).

---

**Note**: This documentation is continuously updated. Please refer to individual section documents for the latest information.