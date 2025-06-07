# Noraneko Development Guide

## 🎯 Getting Started

### Prerequisites

- Deno runtime
- GitHub CLI (`gh`)
- Git

### Quick Setup

```bash
# Clone the repository
git clone https://github.com/your-repo/noraneko.git
cd noraneko

# Install dependencies
deno install --allow-scripts

# Download runtime binaries (Windows)
gh run download -R nyanrus/noraneko-runtime -n noraneko-win-amd64-dev [run_id]

# Start development
deno task dev
```

## 🏗️ Architecture Overview

### Core Components

#### 1. **Build System** (`scripts/build.ts`)

The heart of the development workflow:

- **Development Mode**: Hot Module Replacement (HMR) for rapid iteration
- **Production Mode**: Optimized builds for release
- **Mozbuild Integration**: Works with Firefox's build system

#### 2. **Feature Modules** (`src/apps/main/core/`)

- **Workspaces**: Tab organization and management
- **Split View**: Side-by-side browsing
- **Panel Sidebar**: Customizable sidebar panels

#### 3. **UI Components** (`src/apps/`)

- **Settings**: Browser configuration interface
- **About Pages**: Custom browser pages
- **Themes**: Visual customization system

## 🔧 Development Workflows

### Adding a New Feature

1. **Create Feature Directory**
   ```
   src/apps/main/core/common/your-feature/
   ├── index.ts              # Main entry point
   ├── manager.tsx           # Feature manager
   ├── data/                 # Data management
   └── utils/                # Utilities
   ```

2. **Implement Feature Interface**
   ```typescript
   import { noraComponent, NoraComponentBase } from "@core/utils/base";

   @noraComponent(import.meta.hot)
   export default class YourFeature extends NoraComponentBase {
     init() {
       // Initialize your feature
     }
   }
   ```

3. **Register Feature** Add to the appropriate module loader

### Development Server Ports

| Application | Port | Purpose               |
| ----------- | ---- | --------------------- |
| Main App    | 5181 | Core browser features |
| Designs     | 5182 | Theme development     |
| Settings    | 5183 | Settings interface    |
| Tests       | 5191 | Test environment      |

### Hot Module Replacement (HMR)

HMR is supported for:

- ✅ TypeScript/JavaScript files
- ✅ CSS/SCSS files
- ✅ React/Solid components
- ❌ System modules (require restart)

## 🎨 UI Development

### Creating Components

1. **Solid.js Components** (for browser UI)
   ```typescript
   import { createSignal } from "solid-js";

   export function MyComponent() {
     const [count, setCount] = createSignal(0);

     return (
       <div onClick={() => setCount((c) => c + 1)}>
         Count: {count()}
       </div>
     );
   }
   ```

2. **React Components** (for settings pages)
   ```typescript
   import { useState } from "react";

   export function MySettingsComponent() {
     const [value, setValue] = useState("");

     return (
       <input
         value={value}
         onChange={(e) => setValue(e.target.value)}
       />
     );
   }
   ```

### Styling Guidelines

- Use CSS custom properties for themes
- Follow BEM naming convention
- Leverage Tailwind CSS for settings pages

## 🧪 Testing

### Running Tests

```bash
# Unit tests
deno task test

# Integration tests
deno task test:integration

# E2E tests
deno task test:e2e
```

### Writing Tests

```typescript
import { describe, expect, it } from "vitest";

describe("MyFeature", () => {
  it("should work correctly", () => {
    expect(true).toBe(true);
  });
});
```

## 🔨 Build System

### Build Modes

1. **Development** (`deno task dev`)
   - Fast rebuilds
   - HMR enabled
   - Source maps
   - Debug symbols

2. **Production** (`deno task build`)
   - Optimized output
   - Minification
   - Tree shaking
   - Asset optimization

### Build Phases

1. **Before Mozbuild**: Prepare assets for Firefox build system
2. **After Mozbuild**: Final injection and packaging

## 📦 Package Management

### Adding Dependencies

For Deno projects:

```bash
# Add to deno.json
{
  "imports": {
    "my-library": "npm:my-library@^1.0.0"
  }
}
```

For Node.js projects:

```bash
npm install my-package
```

## 🐛 Debugging

### Browser Debugging

1. Enable Developer Tools in browser
2. Use `console.log()` for quick debugging
3. Use browser debugger for breakpoints

### Build Debugging

1. Check build logs in terminal
2. Verify file paths and imports
3. Use `--verbose` flag for detailed output

### Common Issues

| Issue               | Solution                            |
| ------------------- | ----------------------------------- |
| Build fails         | Check import paths and dependencies |
| HMR not working     | Restart dev server                  |
| Browser won't start | Check binary download               |

## 📚 Resources

- [Mozilla Firefox Developer Documentation](https://firefox-source-docs.mozilla.org/)
- [Deno Documentation](https://deno.land/manual)
- [Vite Documentation](https://vitejs.dev/)
- [Solid.js Documentation](https://docs.solidjs.com/)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

### Code Style

- Use TypeScript for all new code
- Follow ESLint configuration
- Add JSDoc comments for public APIs
- Use conventional commit messages
