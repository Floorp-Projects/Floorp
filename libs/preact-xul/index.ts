// SPDX-License-Identifier: MPL-2.0
import {
  type ComponentChildren,
  h,
  options,
  render as preactRender,
  type VNode,
} from "preact";
import { addDisposer } from "./lifetime.ts";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const factoryInstalled = Symbol.for("@nora/preact-xul/createElementNS");
const vnodeInstalled = Symbol.for("@nora/preact-xul/vnode");
const xulEvents = new Set([
  "onCommand",
  "onPopupShowing",
  "onPopupShown",
  "onPopupHiding",
  "onPopupHidden",
]);

type XULDocument = Document & {
  createXULElement?: (
    name: string,
    options?: ElementCreationOptions,
  ) => Element;
  [factoryInstalled]?: { current: VNode | null };
};

/**
 * Preact creates DOM nodes through document.createElementNS. Handle only our
 * explicit xul: prefix here, before Preact assigns properties/listeners/refs.
 * Replacing an HTML node in a ref instead leaves Preact diffing a detached node
 * and loses its event listeners. This adapter does not touch private VNode fields.
 * The per-document marker prevents wrapping the factory again on HMR.
 */
function installXULFactory(doc: XULDocument): void {
  if (doc[factoryInstalled]) return;
  const state = { current: null as VNode | null };
  const createElementNS = doc.createElementNS;
  doc.createElementNS = function (
    this: Document,
    namespace: string | null,
    qualifiedName: string,
    creationOptions?: ElementCreationOptions | string,
  ): Element {
    const renderingThisNode = state.current?.type === qualifiedName;
    // Consume before native constructors run so their own DOM factory calls
    // retain the browser's requested namespace.
    if (renderingThisNode) state.current = null;
    if (qualifiedName.startsWith("xul:")) {
      const name = qualifiedName.slice(4);
      const xulDoc = this as XULDocument;
      return xulDoc.createXULElement
        ? xulDoc.createXULElement(
          name,
          typeof creationOptions === "object" ? creationOptions : undefined,
        )
        : createElementNS.call(this, XUL_NS, name, creationOptions);
    }
    return createElementNS.call(
      this,
      renderingThisNode && namespace === XUL_NS ? HTML_NS : namespace,
      qualifiedName,
      creationOptions,
    );
  } as Document["createElementNS"];
  doc[factoryInstalled] = state;
}

function patchVNode(vnode: VNode): void {
  if (typeof vnode.type !== "string" || !vnode.type.startsWith("xul:")) return;
  installXULFactory(document);

  // XUL command/popup events are lowercase even when no on* property exists on
  // the element. Preact otherwise preserves the casing of unknown DOM events.
  const props = vnode.props as Record<string, unknown>;
  // Preact reserves key for reconciliation, but XUL key elements also need the
  // native key attribute for keyboard shortcuts.
  if (vnode.type === "xul:key" && vnode.key != null) props.key = vnode.key;
  for (const name of Object.keys(props)) {
    const baseName = name.endsWith("Capture") ? name.slice(0, -7) : name;
    if (xulEvents.has(baseName)) {
      const normalized = name !== baseName
        ? `${baseName.toLowerCase()}Capture`
        : name.toLowerCase();
      props[normalized] = props[name];
      delete props[name];
    }
  }
}

// Keep one link in the shared options hook chain even when this module reloads.
const hookedOptions = options as typeof options & {
  // Preact 10's before-diff/error hooks (also used by its signals/hooks addons).
  // No private VNode DOM/component fields are inspected or rewritten.
  __b?: (vnode: VNode) => void;
  __e?: (
    error: unknown,
    vnode: VNode,
    oldVNode?: VNode,
    info?: unknown,
  ) => void;
  [vnodeInstalled]?: { patch: typeof patchVNode };
};
if (hookedOptions[vnodeInstalled]) {
  hookedOptions[vnodeInstalled].patch = patchVNode;
} else {
  const previousVNode = options.vnode;
  const previousBeforeDiff = hookedOptions.__b;
  const previousDiffed = options.diffed;
  const previousError = hookedOptions.__e;
  const state = { patch: patchVNode };
  hookedOptions[vnodeInstalled] = state;
  options.vnode = (vnode) => {
    previousVNode?.(vnode);
    state.patch(vnode);
  };
  // Independently scheduled child components inherit their actual XUL parent's
  // namespace in Preact. Scope the HTML correction to the exact host VNode being
  // diffed; native browser createElementNS(XUL_NS, name) calls stay untouched.
  const setCreatingVNode = (vnode: VNode | null) => {
    const factory = (document as XULDocument)[factoryInstalled];
    if (factory) factory.current = vnode;
  };
  hookedOptions.__b = (vnode) => {
    previousBeforeDiff?.(vnode);
    setCreatingVNode(vnode);
  };
  options.diffed = (vnode) => {
    setCreatingVNode(null);
    previousDiffed?.(vnode);
  };
  hookedOptions.__e = (error, vnode, oldVNode, info) => {
    setCreatingVNode(null);
    if (previousError) previousError(error, vnode, oldVNode, info);
    else throw error;
  };
}

export * from "preact";
export { createRoot, createRootHMR, disposeRoot } from "./lifetime.ts";

export type RenderContent = ComponentChildren | (() => ComponentChildren);
type RenderOptions = { marker?: Node | null };

/**
 * Insert an independent Preact root among existing browser-owned children.
 * The root is a DOM adapter, not an element: XUL menus, popupsets, flex layouts
 * and direct-child CSS selectors must see the actual rendered children.
 * Factories are Preact components so signal reads subscribe to rerenders.
 */
export function safeRender(
  content: RenderContent,
  container: Element,
  markerOrOptions?: Node | null | RenderOptions,
): () => void {
  const marker = markerOrOptions && "nodeType" in markerOrOptions
    ? markerOrOptions
    : markerOrOptions?.marker ?? null;
  const doc = container.ownerDocument;
  const anchor = doc.createComment("preact-xul root");
  container.insertBefore(anchor, marker);
  const nodes = new WeakSet<Node>();
  const root = {
    nodeType: 1,
    ownerDocument: doc,
    parentNode: container,
    namespaceURI: HTML_NS,
    get childNodes() {
      return Array.from(container.childNodes).filter((node) => nodes.has(node));
    },
    get firstChild() {
      return this.childNodes[0] ?? null;
    },
    insertBefore(node: Node, before: Node | null) {
      // Preact may walk nextSibling beyond this root's comment boundary. Keep
      // insertions inside the owned range instead of crossing a foreign sibling.
      const target = before && nodes.has(before) ? before : anchor;
      container.insertBefore(node, target);
      nodes.add(node);
      return node;
    },
  };
  // Preact's public render type expects a DOM container; its root-fragment
  // protocol only needs the fields above plus its own stored VNode tree.
  const renderRoot = root as unknown as Element;
  let disposed = false;
  const dispose = () => {
    if (disposed) return;
    disposed = true;
    try {
      preactRender(null, renderRoot);
    } finally {
      for (const node of Array.from(container.childNodes)) {
        if (nodes.has(node)) container.removeChild(node);
      }
      anchor.remove();
    }
  };
  try {
    preactRender(
      typeof content === "function"
        ? h(content as () => ComponentChildren, {})
        : content,
      renderRoot,
    );
  } catch (error) {
    dispose();
    throw error;
  }
  addDisposer(dispose);
  return dispose;
}

// Preserve the insertion and cleanup contract of the former Solid renderer.
export const render = safeRender;
