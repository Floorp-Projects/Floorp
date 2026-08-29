// SPDX-License-Identifier: MPL-2.0

export interface NativeSlotMergeResult {
  changed: boolean;
  originalOrder: Element[];
}

function uniqueSameParentElements(
  parent: Element,
  requestedOrder: readonly Element[],
): Element[] | null {
  const unique: Element[] = [];
  const seen = new Set<Element>();
  for (const element of requestedOrder) {
    if (seen.has(element)) continue;
    if (element.parentElement !== parent) return null;
    seen.add(element);
    unique.push(element);
  }
  return unique;
}

function arraysEqual(
  first: readonly Element[],
  second: readonly Element[],
): boolean {
  return first.length === second.length &&
    first.every((element, index) => element === second[index]);
}

/**
 * Reorders only the slots currently occupied by the requested managed nodes.
 * Every unmanaged child keeps its exact slot and relative order.
 */
export function mergeElementsIntoNativeSlots(
  parent: Element,
  requestedOrder: readonly Element[],
): NativeSlotMergeResult {
  const desired = uniqueSameParentElements(parent, requestedOrder);
  if (!desired || desired.length < 2) {
    return { changed: false, originalOrder: desired ?? [] };
  }

  const managed = new Set(desired);
  const originalOrder = Array.from(parent.children).filter((child) =>
    managed.has(child)
  );
  if (originalOrder.length !== desired.length) {
    return { changed: false, originalOrder: [] };
  }
  if (arraysEqual(originalOrder, desired)) {
    return { changed: false, originalOrder };
  }

  const placeholders: Comment[] = [];

  try {
    for (const element of originalOrder) {
      const placeholder = parent.ownerDocument.createComment(
        `floorp-context-slot:${element.id}`,
      );
      parent.insertBefore(placeholder, element);
      placeholders.push(placeholder);
    }
    for (const element of originalOrder) element.remove();
    for (let index = 0; index < desired.length; index++) {
      placeholders[index].replaceWith(desired[index]);
    }
    return { changed: true, originalOrder };
  } catch (error) {
    console.error("[ContextMenuCustomizer] Native-slot merge failed", error);
    for (let index = 0; index < placeholders.length; index++) {
      const placeholder = placeholders[index];
      if (placeholder.parentNode === parent) {
        placeholder.replaceWith(originalOrder[index]);
      }
    }
    for (const element of originalOrder) {
      if (element.parentElement !== parent) parent.appendChild(element);
    }
    return { changed: false, originalOrder: [] };
  } finally {
    for (const placeholder of placeholders) placeholder.remove();
  }
}

/**
 * Restores the still-present members of a previously captured native order.
 *
 * Firefox may remove, replace, or reparent menu nodes while a popup is open.
 * Those nodes must not be reinserted by the overlay rollback, but their absence
 * must not prevent the remaining managed nodes from returning to native order.
 */
export function restoreElementsToNativeSlots(
  parent: Element,
  nativeOrder: readonly Element[],
): NativeSlotMergeResult {
  const survivingOrder = nativeOrder.filter((element) =>
    element.parentElement === parent
  );
  return mergeElementsIntoNativeSlots(parent, survivingOrder);
}
