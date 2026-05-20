# AGENTS.md

Floorp is a Firefox-based web browser ("Noraneko") built on top of the Gecko engine. This repo contains the custom browser features that overlay Firefox — not the Firefox source itself.

## Commands

```bash
deno install                        # Install dependencies
deno task feles-build dev           # Dev mode with HMR (ports 5173–5186)
deno task feles-build build         # Production build (two-phase: --phase before-mach / --phase after-mach)
deno task feles-build stage         # Staging build
deno task feles-build test          # Launch browser with Marionette for automated testing
deno task feles-build misc patch    # Manage Firefox source patches (apply/create/init)
deno task test                      # Run unit tests (browser-integrated, uses colocated test runner)
deno task test -- --near <path>     # Run tests near a specific file/directory
deno task test -- --layer chrome    # Run only chrome-layer tests (also: esm, pages, all)
deno task test:host                 # Run tool tests (Deno native)
deno task test:smoke                # Smoke tests
deno task dev-tool                  # Development utility CLI
```

## Architecture (5 Layers)

```text
Firefox Base  <- patched Gecko, pref overrides (static/gecko/pref/override.ini)
     |
ESM Modules   <- .sys.mts files with direct Firefox API access, Window Actors
     |
Bridge        <- startup scripts that load features from Vite dev servers (HTTP) or chrome://noraneko/ (prod)
     |
Chrome UI     <- SolidJS -> XUL via custom solid-xul renderer, auto-discovered by import.meta.glob
     |
Pages         <- React + Tailwind full-page UIs (settings, new tab, welcome, notes, etc.)
```

The bridge (`bridge/startup/src/chrome_root.ts`) is the bootstrap: in dev/test mode it loads from `http://localhost:5181/loader/index.ts` with retry logic; in production it loads `chrome://noraneko/content/core.js`.

## Tech Stack

- **Runtime**: Deno 2.x (primary), Node.js 22 (some pages)
- **Browser Chrome UI**: SolidJS + `@nora/solid-xul` (custom renderer that uses `document.createXULElement()` via `solid-js/universal`'s `createRenderer`)
- **Settings/Pages UI**: React + Tailwind CSS
- **Build**: Vite via custom `feles-build` system (`tools/feles-build.ts`)
- **Language**: TypeScript (strict)

## Path Aliases (deno.json)

```typescript
#i18n/              -> ./i18n/
#chrome/            -> ./chrome/
#libs/              -> ./libs/
#features-chrome/   -> ./browser-features/chrome/
#modules/           -> ./browser-features/modules/
#ui/                -> ./src/ui/
#themes/            -> ./src/themes/
```

## Coding Conventions

### File Placement

| What               | Where                                                         | Framework   |
| ------------------ | ------------------------------------------------------------- | ----------- |
| Browser UI feature | `browser-features/chrome/common/{name}/`                      | SolidJS     |
| Firefox API module | `browser-features/modules/modules/{name}.sys.mts`             | Firefox ESM |
| Settings page      | `browser-features/pages-settings/src/app/{name}/`             | React       |
| Actor (IPC)        | `browser-features/modules/actors/{Name}Parent\|Child.sys.mts` | Firefox ESM |

### Feature Auto-Discovery

`browser-features/chrome/common/mod.ts` uses `import.meta.glob("./*/index.ts")` -- adding a directory with an `index.ts` under `common/` is sufficient. No manual registration needed.

Actor registration is manual: add entries to the `JS_WINDOW_ACTORS` object in `browser-features/modules/modules/BrowserGlue.sys.mts`.

### Type Definitions

Separate types into dedicated `types.ts` files. Exception: `.sys.mts` files may include inline types.

### No `any`

Never use `any`. Use explicit types or `unknown`.

### SolidJS Pattern (Browser Chrome)

```typescript
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { render } from "@nora/solid-xul";
import { createSignal } from "solid-js";

@noraComponent(import.meta.hot)
export default class MyFeature extends NoraComponentBase {
  init(): void {
    /* ... */
  }
}
```

- Always use `@noraComponent(import.meta.hot)` decorator for HMR support
- Use `createSignal` / `createMemo` for reactive state -- never mutate variables directly

### Firefox ESM Modules (.sys.mts)

```typescript
const { SomeService } = ChromeUtils.importESModule(
  "resource://floorp/lib/SomeService.sys.mjs",
);
```

### React Pattern (Settings Pages)

```typescript
export default function MyPage() {
  const [value, setValue] = useState("");
  return <div className="p-4">...</div>;
}
```

### i18n

Translations use i18next-style `{{variable}}` interpolation and `_plural` suffix keys. Add strings to at least `i18n/en-US/` and `i18n/ja-JP/`. Organized by feature namespace in `browser-chrome.json`.

### Error Handling

Use try/catch with `console.error("[FeatureName]", ...)` prefix for log messages.

## Testing

Tests run **inside the actual Firefox browser**, not in Deno/Node. They are plain TypeScript functions with a custom harness (`test_harness.ts` providing `assert`, `assertEquals`, `TestCase`), not `Deno.test()`.

```typescript
// @colocated-env browser
import { type TestCase, assert } from "../../../test/utils/test_harness.ts";

function testSomething(): void {
  assert(condition, "message");
}

export function runAllTests(): void {
  const tests: TestCase[] = [{ name: "something works", fn: testSomething }];
  // ...run and collect failures
}
```

Colocated in `test/` directories next to source. The runner discovers them automatically.

## Dev Server Ports

- 5173: Main features
- 5174-5177: Other pages
- 5178: Settings pages
- 5181: Feature loader (bridge)
- 5186: New tab page

## dev-tool -- Browser Inspection CLI

`deno task dev-tool` communicates with a running Floorp instance via the **Marionette protocol** (Firefox's TCP-based WebDriver). All browser commands require the browser to be running (started via `dev-tool start` or `feles-build dev`).

### Process Management

```bash
deno task dev-tool start      # Start dev server + browser in background (waits for Marionette ready)
deno task dev-tool stop       # Kill all dev processes (deno, vite, floorp) cleanly
deno task dev-tool restart    # Stop then start
deno task dev-tool rebuild    # Rebuild startup + modules + inject XHTML without restarting browser (HMR handles loader-features)
```

### Browser Commands

```bash
deno task dev-tool status                                      # Check connection: shows page title, URL, tab count, active tab
deno task dev-tool eval "JSON.stringify(Services.prefs.getStringPref('floorp.some.pref'))"  # Execute JS in browser (returns result)
deno task dev-tool console                                     # Last 50 console messages (via Services.console)
deno task dev-tool console -l 100 -f "workspace" -l error      # Filtered: 100 messages, text filter "workspace", errors only
deno task dev-tool console --level error                       # Level filter: error/warn/info/debug/all
deno task dev-tool screenshot                                  # Full-page screenshot -> _dist/screenshot.png
deno task dev-tool screenshot -s "#tabbrowser-tabs" -o out.png # Element screenshot by CSS selector
deno task dev-tool navigate about:preferences                  # Navigate browser to URL
deno task dev-tool dom "#sidebar-box"                          # Inspect DOM: tag, id, class, text, attributes, child count
deno task dev-tool title                                       # Get current page title
```

### Context Flag (`--context` / `-c`)

All browser commands accept `--context` to choose the JS execution context:

- `chrome` (default) -- browser chrome scope. Access `Services`, `gBrowser`, XUL elements, Firefox internals, prefs
- `content` -- web content scope. Interact with loaded page DOM as a normal web page

```bash
deno task dev-tool eval "gBrowser.tabs.length" -c chrome    # Count open tabs (chrome context)
deno task dev-tool eval "document.title" -c content         # Get page title from content context
```

### Typical LLM Workflow

1. `deno task dev-tool start` -- launch browser
2. Edit source files (HMR updates chrome UI automatically)
3. `deno task dev-tool console --level error` -- check for runtime errors
4. `deno task dev-tool eval "..."` -- inspect live state (prefs, DOM, services)
5. `deno task dev-tool screenshot` -- visually verify UI changes
6. `deno task dev-tool rebuild` -- if HMR didn't pick up changes (modules/startup only)
7. `deno task dev-tool stop` -- shut down when done

## Debugging

- Browser Console: `Ctrl+Shift+J` / `Cmd+Option+J`
- DevTools: `Ctrl+Shift+I` / `Cmd+Option+I`
- Log convention: `console.log("[FeatureName]", ...)`

## Further Reading

- `docs/llm/project-overview.md` -- Full project overview
- `docs/llm/architecture-deep-dive.md` -- 5-layer architecture, Window Actor patterns, build system internals
- `docs/llm/development-notes.md` -- Setup requirements, .sys.mts vs .ts decision matrix, dev-tool CLI
