// SPDX-License-Identifier: MPL-2.0

import {
  mergeElementsIntoNativeSlots,
  restoreElementsToNativeSlots,
} from "./order-policy.ts";
import type {
  ContextMenuRegistry,
  ResolvedContextMenuSurface,
} from "./registry.ts";
import { findSeparatorsToHide } from "./separator-policy.ts";
import {
  acquireContextMenuStyle,
  type ContextMenuStyleLease,
  FLOORP_CONTEXT_HIDDEN_ATTRIBUTE,
  FLOORP_CONTEXT_SEPARATOR_HIDDEN_ATTRIBUTE,
  FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE,
} from "./style.ts";
import type { EffectiveContextMenuLevelOverride } from "./types.ts";

interface AttributeRecord {
  element: Element;
  name: string;
}

interface ReorderRecord {
  parent: Element;
  originalOrder: Array<{ element: Element; key: string }>;
}

interface HiddenPropertyRecord {
  element: Element & { hidden: boolean };
  value: boolean;
}

function uniqueResolvedElements(
  surface: ResolvedContextMenuSurface,
  registry: ContextMenuRegistry,
  keys: readonly string[],
): Element[] {
  const result: Element[] = [];
  const seen = new Set<Element>();
  for (const key of keys) {
    const resolution = registry.resolveItem(surface, key);
    if (resolution.status !== "resolved") continue;
    if (seen.has(resolution.element)) continue;
    seen.add(resolution.element);
    result.push(resolution.element);
  }
  return result;
}

function uniqueResolvedOrderItems(
  surface: ResolvedContextMenuSurface,
  registry: ContextMenuRegistry,
  keys: readonly string[],
): Array<{ element: Element; key: string }> {
  const result: Array<{ element: Element; key: string }> = [];
  const seen = new Set<Element>();
  for (const key of keys) {
    const resolution = registry.resolveItemForOrdering(surface, key);
    if (resolution.status !== "resolved") continue;
    if (seen.has(resolution.element)) continue;
    seen.add(resolution.element);
    result.push({ element: resolution.element, key });
  }
  return result;
}

export class ContextMenuTransaction {
  readonly #surface: ResolvedContextMenuSurface;
  readonly #registry: ContextMenuRegistry;
  readonly #level: EffectiveContextMenuLevelOverride;
  readonly #attributes: AttributeRecord[] = [];
  readonly #reorders: ReorderRecord[] = [];
  readonly #hiddenProperties: HiddenPropertyRecord[] = [];
  #styleLease: ContextMenuStyleLease | null = null;
  #applied = false;
  #rolledBack = false;

  constructor(
    surface: ResolvedContextMenuSurface,
    registry: ContextMenuRegistry,
    level: EffectiveContextMenuLevelOverride,
  ) {
    this.#surface = surface;
    this.#registry = registry;
    this.#level = level;
  }

  apply(): boolean {
    if (this.#applied || this.#rolledBack) return false;
    this.#applied = true;

    try {
      let changed = false;
      const hiddenElements = uniqueResolvedElements(
        this.#surface,
        this.#registry,
        this.#level.hidden,
      );
      for (const element of hiddenElements) {
        changed = this.setTransientAttribute(
          element,
          FLOORP_CONTEXT_HIDDEN_ATTRIBUTE,
        ) || changed;
      }

      const orderedItems = uniqueResolvedOrderItems(
        this.#surface,
        this.#registry,
        this.#level.order,
      );
      const byParent = new Map<
        Element,
        Array<{ element: Element; key: string }>
      >();
      for (const item of orderedItems) {
        const element = item.element;
        const parent = element.parentElement;
        if (!parent) continue;
        const group = byParent.get(parent) ?? [];
        group.push(item);
        byParent.set(parent, group);
      }

      for (const [parent, desiredItems] of byParent) {
        const result = mergeElementsIntoNativeSlots(
          parent,
          desiredItems.map((item) => item.element),
        );
        if (!result.changed) continue;
        const keyByElement = new Map(
          desiredItems.map((item) => [item.element, item.key]),
        );
        this.#reorders.push({
          parent,
          originalOrder: result.originalOrder.flatMap((element) => {
            const key = keyByElement.get(element);
            return key ? [{ element, key }] : [];
          }),
        });
        changed = true;
      }

      // Separator cleanup is an overlay too, and only runs when an actual user
      // delta was applied. Unknown/missing keys therefore remain a pure no-op.
      if (changed) {
        for (const element of Array.from(this.#surface.popup.children)) {
          if (
            element.localName !== "menuseparator" ||
            !element.hasAttribute(FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE)
          ) {
            continue;
          }
          const separator = element as Element & { hidden: boolean };
          this.#hiddenProperties.push({
            element: separator,
            value: separator.hidden,
          });
          separator.hidden = false;
        }
        for (const separator of findSeparatorsToHide(this.#surface.popup)) {
          this.setTransientAttribute(
            separator,
            FLOORP_CONTEXT_SEPARATOR_HIDDEN_ATTRIBUTE,
          );
        }
      }

      if (this.#attributes.length > 0) {
        this.#styleLease = acquireContextMenuStyle(
          this.#surface.popup.ownerDocument,
        );
      }
      return changed;
    } catch (error) {
      console.error(
        "[ContextMenuCustomizer] Transaction apply failed; rolling back",
        error,
      );
      this.rollback();
      return false;
    }
  }

  rollback(): void {
    if (this.#rolledBack) return;
    this.#rolledBack = true;

    for (let index = this.#reorders.length - 1; index >= 0; index--) {
      const record = this.#reorders[index];
      const seen = new Set<Element>();
      const nativeOrder = record.originalOrder.flatMap((item) => {
        let element = item.element.parentElement === record.parent
          ? item.element
          : null;
        if (!element) {
          const replacement = this.#registry.resolveItemForOrdering(
            this.#surface,
            item.key,
          );
          if (
            replacement.status === "resolved" &&
            replacement.element.parentElement === record.parent
          ) {
            element = replacement.element;
          }
        }
        if (!element || seen.has(element)) return [];
        seen.add(element);
        return [element];
      });
      restoreElementsToNativeSlots(record.parent, nativeOrder);
    }
    this.#reorders.length = 0;

    for (let index = this.#attributes.length - 1; index >= 0; index--) {
      const record = this.#attributes[index];
      record.element.removeAttribute(record.name);
    }
    this.#attributes.length = 0;
    for (let index = this.#hiddenProperties.length - 1; index >= 0; index--) {
      const record = this.#hiddenProperties[index];
      record.element.hidden = record.value;
    }
    this.#hiddenProperties.length = 0;
    this.#styleLease?.release();
    this.#styleLease = null;
  }

  private setTransientAttribute(element: Element, name: string): boolean {
    if (element.hasAttribute(name)) return false;
    element.setAttribute(name, "true");
    this.#attributes.push({ element, name });
    return true;
  }
}
