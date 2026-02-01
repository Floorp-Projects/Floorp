# AI Coding Agent Instructions

This workspace contains multiple interconnected projects: **Floorp** (Firefox-based browser), **Sapphillon** (workflow automation backend), **Floorp-OS-Automator-Frontend**, and **web-store**.

## Project Overview

### Floorp (`floorp/`)
Firefox-based browser built with Deno 2.x, SolidJS, and React. Custom browser features overlay Firefox's Gecko engine.

### Sapphillon Backend (`Floorp-OS-Automator-Backend/`)
Rust-based workflow automation backend using gRPC (tonic), SeaORM, and Deno runtime for executing JavaScript workflows.

### Frontend Projects
- **Floorp-OS-Automator-Frontend**: React + TypeScript + Vite UI for workflow management
- **web-store**: Plugin marketplace with React + Vite

---

## Cross-Project Integration

This workspace contains interconnected projects. Understanding data flow and integration points is crucial for full-stack development.

### Data Flow Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    Floorp Browser                        │
│              (Mozilla Firefox + Custom Features)           │
│  Location: /Users/user/dev-source/floorp-dev/floorp/     │
│  Dev: deno task dev (ports: 5173-5186)              │
└───────────────────────────┬────────────────────────────────┘
                          │
                          │ Floorp OS API (OpenAPI)
                          │ openapi.yaml:
                          │ Floorp-OS-Automator-Backend/plugins/floorp/
                          │
                          ▼
┌──────────────────────────────────────────────────────────────┐
│              Sapphillon Backend (Rust)                │
│  Location: ../Floorp-OS-Automator-Backend/          │
│  Dev: make run (gRPC on localhost:50051)            │
│  - Plugin system (floorp, fetch, filesystem, etc.)    │
│  - Workflow engine (Deno Core runtime)                 │
└───────────────────────────┬────────────────────────────────┘
                          │
                          │ gRPC (tonic + prost)
                          │ protobuf in:
                          │ Floorp-OS-Automator-Frontend/vender/Sapphillon_API/
                          │
                          ▼
┌──────────────────────────────────────────────────────────────┐
│         Floorp-OS-Automator-Frontend (React)          │
│  Location: ../Floorp-OS-Automator-Frontend/          │
│  Dev: pnpm dev (port 8081)                          │
│  - gRPC-Web client (@connectrpc/connect-web)           │
│  - Workflow management UI                                │
└───────────────────────────┬────────────────────────────────┘
                          │
                          │ (optional, disabled)
                          │ PostMessage to Floorp
                          │ Progress Window
                          ▼
                   ┌────────────────┐
                   │  Floorp       │
                   │  Progress     │
                   │  Window       │
                   └────────────────┘

                          ▲
                          │
                ┌─────────┴──────────┐
                │                    │
┌───────────────┐      ┌────────────────┐
│  Web Store    │      │  External     │
│  (Plugin      │      │  Plugins     │
│   Marketplace)│      │              │
└───────────────┘      └────────────────┘
```

### Integration Points

**1. Floorp → Sapphillon** (Browser Automation):
- Floorp exposes browser automation via OpenAPI spec at `Floorp-OS-Automator-Backend/plugins/floorp/api-spec/openapi.yaml`
- Sapphillon's `floorp` plugin consumes this API to control browser tabs, navigation, etc.
- Workflows can automate browser interactions (open tabs, navigate, fill forms)

**2. Sapphillon → Frontend** (Workflow Management):
- gRPC communication using `tonic` (server) and `@connectrpc/connect-web` (client)
- Protobuf definitions in `Floorp-OS-Automator-Frontend/vender/Sapphillon_API/proto/`
- Frontend generates TypeScript clients from `.proto` files
- Services: Workflow, Plugin, Version, Model, Provider

**3. Frontend → Floorp** (Progress Tracking):
- Disabled by default (see `lib/workflow-progress.ts`)
- Uses `window.OSAutomotor?.sendWorkflowProgress()` for real-time updates
- Would show workflow progress in Floorp's native progress window

**4. Web Store → Sapphillon** (Plugin Distribution):
- Plugin marketplace at `../web-store/`
- Plugins installed via Sapphillon's plugin installer
- External plugins run in separate processes with gRPC communication

### Development Setup for Full Stack

**Recommended workflow for full-stack development**:
```bash
# Terminal 1: Start Floorp Browser
cd /Users/user/dev-source/floorp-dev/floorp
deno task dev

# Terminal 2: Start Sapphillon Backend
cd /Users/user/dev-source/sapphillon-dev/Floorp-OS-Automator-Backend
make run  # Starts gRPC server on localhost:50051

# Terminal 3: Start Frontend (production mode)
cd /Users/user/dev-source/sapphillon-dev/Floorp-OS-Automator-Frontend
pnpm dev:prod  # Connects to localhost:50051
```

**Frontend-only development** (no backend needed):
```bash
cd /Users/user/dev-source/sapphillon-dev/Floorp-OS-Automator-Frontend
pnpm dev:mock  # Mock gRPC on port 50099, Vite on 5199
```

---

## Floorp Development Patterns

### Build System
- **Primary**: `deno task dev` - Runs custom feles-build system
- **Build orchestration**: `tools/feles-build.ts` orchestrates patches, symlinks, Vite dev servers
- **Dev server ports**: 5173 (main), 5178 (settings), 5186 (newtab), 5174-5177 (other pages)

### Code Organization

**Type Definitions**: Always separate types into dedicated files (`types.ts`), not mixed with implementation.
```typescript
// Good: types.ts
export interface Config { enabled: boolean; }

// index.ts
import type { Config } from "./types.ts";
```
**Exception**: `.sys.mts` files can include types with Firefox API integration.

**File Extensions**:
- `.ts` - General TypeScript
- `.sys.mts` - Firefox ESM modules (loaded via `ChromeUtils.importESModule()`)
- `.jsx` - React components in pages-*/ directories

### UI Framework Usage

**SolidJS** (`browser-features/chrome/`):
- Use `createSignal` for reactive state (never mutate directly)
- `@noraComponent(import.meta.hot)` decorator for HMR support
- Import from `@nora/solid-xul` for rendering:
```typescript
import { render, createSignal } from "@nora/solid-xul";
const [count, setCount] = createSignal(0);
setCount(c => c + 1);
```

**React** (`browser-features/pages-*/`):
- Functional components with hooks
- Tailwind CSS for styling
- `import { createRootHMR } from "@nora/solid-xul"` for i18n integration

### Firefox API Integration
Access Firefox APIs via ESM modules in `.sys.mts` files:
```typescript
const { I18nUtils } = ChromeUtils.importESModule("resource://floorp/lib/I18nUtils.sys.mjs");
```

---

## Sapphillon Backend Patterns

### Architecture
- **gRPC Service Layer**: `src/services/` - Tonic-based services (workflow, plugin, model, version, provider)
- **Plugin System**: Modular plugins in `plugins/` (fetch, filesystem, floorp, vscode, git, finder, etc.)
- **Database**: SeaORM with SQLite, migrations in `migration/`, entities in `entity/`
- **Workflow Engine**: Deno Core runtime executes JavaScript workflows with plugin permissions

### Common Commands
```bash
make rust_test          # Run all workspace tests
make rust_build         # Build entire workspace
make migrate            # Run SeaORM migrations
make entity_generate    # Generate entities from DB
make run                # Run with debug DB at ./debug/sqlite.db
make grpcui            # Launch gRPC UI on localhost:50051
```

### Testing
- Unit tests: `cargo test --lib`
- External plugin tests: `cargo test --test external_plugin`
- Ignored tests: `cargo test --lib external_plugin -- --ignored`

### Plugin Development
Each plugin crate exposes Deno-compatible functions. See `plugins/floorp/` for OpenAPI-based Floorp browser control.

**Debug Workflows**: Place JS files in `debug_workflow/` directory - auto-registered every 10s in debug builds with full permissions (`[DEBUG]` prefix).

### gRPC & Protocol Buffers
- Uses `tonic` for server, `prost` for code generation
- `buf.yaml` and `buf.gen.yaml` configuration
- Generate with `buf generate`

---

## Frontend Patterns (Both Projects)

### Build Commands
```bash
npm run dev              # Vite dev server
npm run build            # TypeScript check + Vite build
npm run test             # Vitest
npm run lint             # ESLint
```

### Component Patterns
- **Chakra UI**: Primary component library
- **i18next**: Internationalization via `src/i18n/config.ts`
- **React Router**: Routing in `src/routes/`
- **Type Definitions**: Export from `src/types/`, not inline

### gRPC Communication
- `@connectrpc/connect-web` for browser-based gRPC
- `@connectrpc/connect` for Node-based
- Generated clients in `src/gen/` from proto definitions

---

## Cross-Project Integration

### Floorp OS API
Floorp exposes browser automation via OpenAPI spec in `Floorp-OS-Automator-Backend/plugins/floorp/api-spec/openapi.yaml`. The Sapphillon floorp plugin consumes this for workflow automation.

### Data Flow
```
Floorp Browser
    ↓ (Floorp OS API)
Sapphillon Backend (floorp plugin)
    ↓ (gRPC)
Floorp-OS-Automator-Frontend
```

### Plugin System
Workflows in Sapphillon can invoke plugins including:
- **floorp**: Control Floorp browser (tabs, navigation, etc.)
- **fetch**: HTTP requests
- **filesystem**: File operations
- **vscode**: VS Code control
- **finder**: macOS Finder automation
- And more in `plugins/`

---

## Key Conventions

### Error Handling
- **Floorp**: Use `Result<T, E>` from fp-ts for type-safe error handling
- **Sapphillon**: `anyhow::Result<T>` and `tonic::Status` for gRPC errors

### State Management
- **SolidJS**: `createSignal` - never mutate directly
- **React**: `useState`, `useContext` for global state

### Database Operations
- Use SeaORM `Entity::find().all(db).await?` pattern
- Migrations: `sea-orm-cli migrate generate <name>`
- Entities are auto-generated - don't edit manually

### Permissions
Sapphillon uses wildcard `*` plugin_function_id to grant workflows full access. Used for trusted workflows and testing.

---

## Language-Specific Notes

### Rust
- Edition 2024, workspace resolver "3"
- Async/await with tokio runtime
- `#[tonic::async_trait]` for gRPC service implementations

### TypeScript/Deno
- No `any` types - use explicit types or `unknown`
- Null/undefined checks required
- Deno permissions: `--allow-all` often needed for build scripts

### React
- Functional components only
- Hooks over class components
- Tailwind CSS classes (e.g., `className="p-4"`)

---

## Critical Files Reference

| Purpose | File |
|---------|------|
| Floorp build system | `floorp/tools/feles-build.ts` |
| Floorp dev config | `floorp/deno.json` |
| Sapphillon main entry | `Floorp-OS-Automator-Backend/src/main.rs` |
| Sapphillon workflow service | `Floorp-OS-Automator-Backend/src/services/workflow.rs` |
| Frontend Vite config | `Floorp-OS-Automator-Frontend/vite.config.ts` |
| Floorp SolidJS renderer | `floorp/libs/solid-xul/index.ts` |
| Plugin permissions | `Floorp-OS-Automator-Backend/src/services/workflow.rs:make_plugin_permission()` |
