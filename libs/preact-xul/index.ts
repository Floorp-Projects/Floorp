// SPDX-License-Identifier: MPL-2.0
import { options, VNode, Ref, render as preactRender } from "preact";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// DOP: Data transformations & Generators
const createXul = (tag: string) =>
  (document as any).createXULElement?.(tag) ??
  document.createElementNS(XUL_NS, tag);

const migrate = (from: Element, to: Element) => {
  Array.from(from.attributes).forEach((a) => to.setAttribute(a.name, a.value));
  while (from.firstChild) to.appendChild(from.firstChild);
  return Object.assign(to, { __isXUL: true });
};

const materialize = (el: Element, tag: string) => {
  const xul = migrate(el, createXul(tag));
  el.parentNode?.replaceChild(xul, el);
  return xul;
};

// FP: Composition & Higher-Order Functions
const commit = <T>(ref: Ref<T> | undefined, val: T | null) =>
  typeof ref === "function" ? ref(val) : ref && (ref.current = val);

const liftRef = (tag: string, orig?: Ref<any>) => (el: Element | null) => {
  if (!el) return commit(orig, el); // Handle null case
  if ((el as any).__isXUL) return commit(orig, el); // Already XUL
  if (!el.parentNode) {
    console.warn("preact-xul: liftRef called with detached element, skipping materialize");
    return;
  }
  return commit(orig, materialize(el, tag)); // Transform to XUL
};

const patch = (vnode: VNode) => {
  if (typeof vnode.type === "string" && vnode.type.startsWith("xul:")) {
    vnode.ref = liftRef((vnode.type = vnode.type.slice(4)), vnode.ref);
  }
};

// NOTE: This module monkey-patches options.vnode globally and affects all Preact
// instances in the same JS context. It must be imported exactly once per JS
// context. Multiple imports will chain patches correctly via prev?.(v), but
// importing this module in multiple contexts will cause duplicate patching.
const prev = options.vnode;
options.vnode = (v) => (patch(v), prev?.(v));

export * from "preact";

/**
 * Non-destructive wrapper around preact.render().
 *
 * `preact.render(vnode, container)` replaces *all* children of `container`
 * with VDOM-managed nodes, destroying pre-existing content (Firefox-internal
 * <link>/<meta> elements, existing XUL children, etc.).
 *
 * `safeRender` instead appends a dedicated `<div style="display:contents">`
 * wrapper inside `container` and renders into that, leaving other children
 * intact.  Pass `marker` to `insertBefore(wrapper, marker)` instead of
 * `appendChild`.
 *
 * Returns a dispose function that unmounts and removes the wrapper.
 */
export function safeRender(
  vnode: VNode | null,
  container: Element,
  marker?: Node | null,
): () => void {
  const doc = container.ownerDocument ?? document;
  const wrapper = doc.createElement("div");
  wrapper.style.display = "contents";
  if (marker) {
    container.insertBefore(wrapper, marker);
  } else {
    container.appendChild(wrapper);
  }
  preactRender(vnode, wrapper);
  return () => {
    preactRender(null, wrapper);
    wrapper.remove();
  };
}
