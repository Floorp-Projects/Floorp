# Preact in browser chrome

Import `render` (or its identical alias `safeRender`) from `@nora/preact-xul`
when inserting UI among Firefox-owned children. Each call creates an independent
root and returns an idempotent unmount function. It inserts actual elements as
direct children, without an HTML wrapper, and accepts either a marker node or
`{ marker }` as its third argument. It does not replace an earlier render root.

Pass a component or a factory to keep signal reads inside Preact rendering:

```tsx
const dispose = render(
  () => <xul:menuitem label={title.value} onCommand={handleCommand} />,
  menu,
  marker,
);
```

Use the `xul:` prefix for XUL elements. Native nodes are created before Preact
sets properties, listeners and refs, so their identity is retained through
updates and unmounts. Ordinary JSX tags remain HTML; SVG retains its namespace.
`onCommand` and popup event names are normalized to native lowercase events;
other custom event names retain their case. Event handlers must be functions,
not inline JavaScript strings. On `xul:key`, the reserved Preact `key` also
becomes the native keyboard-shortcut attribute.

## Lifetimes

`createRoot(fn)` passes an idempotent disposer to its synchronous setup
callback. `addDisposer`, `rootEffect`, nested roots, and render roots created
during setup belong to that scope. Failed setup releases resources before
rethrowing. A failed cleanup is logged and does not prevent other cleanup from
running.

`createRootHMR(fn, hot)` groups all module roots under one Vite dispose
callback. When `hot.data.__preactXulExternalDisposeOwner` is set, the module
owner must call `disposeRoot(hot)` from its own HMR callback. This avoids
overwriting that callback.

Setup scopes do not propagate across asynchronous work or later component
renders. Use Preact `useEffect`/`useLayoutEffect` cleanup inside components, or
explicitly retain the returned disposer for asynchronous resources.
`createNodeDisposer` handles widgets initially created while detached, moves
between connected parents, ancestor removal, and document unload.

## Compatibility boundary and verification

The adapter targets Preact 10. It uses its before-diff (`options.__b`) and error
(`options.__e`) hooks, plus the public `vnode`/`diffed` hooks. The private hook
names are a deliberate compatibility dependency. They scope HTML namespace
correction to the precise element Preact creates during a scheduled component
update under a XUL parent. Browser-native XUL creation remains unchanged. No
private VNode DOM pointers are replaced. The independent-root container relies
on Preact's root-fragment container protocol. Revalidate both mechanisms when
updating Preact; do not assume compatibility with another major version.

The colocated renderer and lifetime tests are also exposed through
`browser-features/chrome/test/unit/preactXul.test.ts`. The migrated
`solidXulNullProps.test.ts` checks native attribute removal, style updates and
event-listener replacement. Tests cover signals, native node identity, refs,
keyed reordering, multiple insertion roots, namespaces, keyboard shortcuts,
cleanup and simulated HMR disposal. These tests do not replace end-to-end
feature testing, the actual Vite HMR flow, or testing against the pinned Floorp
runtime.
