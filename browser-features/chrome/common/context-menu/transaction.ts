// SPDX-License-Identifier: MPL-2.0

import {
  mergeElementsIntoNativeSlots,
  type NativeNodePlacementHint,
  type NativeSlotRecord,
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
  originalSlots: Array<NativeSlotRecord & { key: string }>;
  overlayChildren: readonly Element[];
  mutationObserver: MutationObserver | null;
  mutationRecords: MutationRecord[];
}

interface NativeMutationSummary {
  additions: Set<Element>;
  movedElements: Set<Element>;
  nativeOwners: Map<Element, Element>;
  nativeOrderReplaced: boolean;
  placements: Map<Element, NativeNodePlacementHint>;
  replacements: Map<Element, Element>;
}

interface HiddenPropertyRecord {
  element: Element & { hidden: boolean };
  value: boolean;
}

function reportRollbackFailure(stage: string, error: unknown): void {
  try {
    console.error(
      `[ContextMenuCustomizer] Transaction rollback ${stage} failed`,
      error,
    );
  } catch {
    // Cleanup must remain best-effort even when the logging host is unavailable.
  }
}

function elementNodes(nodes: NodeList): Element[] {
  const result: Element[] = [];
  for (const node of Array.from(nodes)) {
    if (node?.nodeType === 1) result.push(node as Element);
  }
  return result;
}

function mutationPlacement(
  record: MutationRecord,
  overlayIndexByElement: ReadonlyMap<Element, number>,
  overlayLength: number,
  movedElements: ReadonlySet<Element>,
): NativeNodePlacementHint {
  if (!record.previousSibling) return { edge: "start", nativeChildGap: 0 };
  if (!record.nextSibling) {
    return { edge: "end", nativeChildGap: overlayLength };
  }
  const before = record.nextSibling.nodeType === 1
    ? record.nextSibling as Element
    : undefined;
  const after = record.previousSibling.nodeType === 1
    ? record.previousSibling as Element
    : undefined;
  const beforeIndex = before && !movedElements.has(before)
    ? overlayIndexByElement.get(before)
    : undefined;
  const afterIndex = after && !movedElements.has(after)
    ? overlayIndexByElement.get(after)
    : undefined;
  return {
    before,
    after,
    nativeChildGap: beforeIndex !== undefined
      ? beforeIndex
      : afterIndex !== undefined
      ? afterIndex + 1
      : undefined,
  };
}

function summarizeNativeMutations(
  records: readonly MutationRecord[],
  parent: Element,
  overlayChildren: readonly Element[],
): NativeMutationSummary {
  const additions = new Set<Element>();
  const movedElements = new Set<Element>();
  const placements = new Map<Element, NativeNodePlacementHint>();
  const replacements = new Map<Element, Element>();
  const overlayIndexByElement = new Map(
    overlayChildren.map((element, index) => [element, index]),
  );
  const overlaySet = new Set(overlayChildren);
  // Each child owns the native slot it occupied before Floorp applied the
  // overlay. Replay ownership through Firefox mutations instead of inferring
  // it from the final DOM: an existing replacement takes the removed node's
  // current slot, and a later move can transfer that slot again.
  const ownerByElement = new Map(
    overlayChildren.map((element) => [element, element]),
  );
  const elementByOwner = new Map(
    overlayChildren.map((element) => [element, element]),
  );
  const detachNativeOwner = (element: Element): Element | undefined => {
    const owner = ownerByElement.get(element);
    if (!owner) return undefined;
    ownerByElement.delete(element);
    if (elementByOwner.get(owner) === element) elementByOwner.delete(owner);
    return owner;
  };
  const assignNativeOwner = (owner: Element, element: Element): void => {
    detachNativeOwner(element);
    const previousElement = elementByOwner.get(owner);
    if (previousElement) ownerByElement.delete(previousElement);
    ownerByElement.set(element, owner);
    elementByOwner.set(owner, element);
    additions.delete(element);
    movedElements.delete(element);
    placements.delete(element);
  };
  let nativeOrderReplaced = false;
  let pendingWholeParentClear = false;

  const recordReplacement = (
    removedElement: Element,
    replacement: Element,
  ): void => {
    replacements.set(removedElement, replacement);
    // Only anchors recorded before this mutation follow the replacement.
    // Anchors created after a revived original is reinserted keep referring to
    // that original, which preserves mutation chronology.
    for (const [element, placement] of placements) {
      if (
        placement.before !== removedElement &&
        placement.after !== removedElement
      ) {
        continue;
      }
      placements.set(element, {
        ...placement,
        before: placement.before === removedElement
          ? replacement
          : placement.before,
        after: placement.after === removedElement
          ? replacement
          : placement.after,
      });
    }
  };

  const promoteAdjacentAdditions = (
    removedElement: Element,
    nativeOwner?: Element,
  ): void => {
    const currentIndex = new Map(
      Array.from(parent.children).map((element, index) => [element, index]),
    );
    const candidates = Array.from(additions)
      .filter((element) => {
        if (element.parentElement !== parent) return false;
        const placement = placements.get(element);
        return placement?.before === removedElement ||
          placement?.after === removedElement;
      })
      .sort((first, second) =>
        (currentIndex.get(first) ?? Number.MAX_SAFE_INTEGER) -
        (currentIndex.get(second) ?? Number.MAX_SAFE_INTEGER)
      );
    const [replacement, ...followers] = candidates;
    if (!replacement) return;
    if (!nativeOwner) {
      // Contracting a Firefox-added block must not turn its next member into
      // an owner of some vacant native slot. Keep it as an addition and carry
      // the removed addition's frozen placement forward.
      const inheritedPlacement = placements.get(removedElement);
      additions.delete(removedElement);
      movedElements.delete(removedElement);
      placements.delete(removedElement);
      if (inheritedPlacement) {
        for (const follower of candidates) {
          placements.set(follower, inheritedPlacement);
        }
      }
      return;
    }
    recordReplacement(removedElement, replacement);
    additions.delete(replacement);
    movedElements.delete(replacement);
    placements.delete(replacement);
    if (nativeOwner) assignNativeOwner(nativeOwner, replacement);
    for (const follower of followers) {
      placements.set(follower, { after: replacement });
    }
  };

  for (const record of records) {
    if (record.type !== "childList" || record.target !== parent) continue;
    const removed = elementNodes(record.removedNodes);
    const added = elementNodes(record.addedNodes);
    const followsWholeParentClear = pendingWholeParentClear &&
      added.length > 0 && !record.previousSibling && !record.nextSibling;
    pendingWholeParentClear = removed.length > 0 && added.length === 0 &&
      !record.previousSibling && !record.nextSibling;
    const removedOwners = new Map<Element, Element>();
    for (const removedElement of removed) {
      const owner = detachNativeOwner(removedElement);
      if (owner) removedOwners.set(removedElement, owner);
    }
    // Adding an already-owned element is a native move. Its previous slot is
    // vacated before it can inherit a replacement destination below.
    for (const addedElement of added) detachNativeOwner(addedElement);
    const addedOverlaySubsequence = added.filter((element) =>
      overlaySet.has(element)
    );
    const identityPreservingInsertion = added.length > 0 &&
      !record.previousSibling &&
      !record.nextSibling &&
      addedOverlaySubsequence.length === overlayChildren.length &&
      addedOverlaySubsequence.every((element, index) =>
        element === overlayChildren[index]
      );
    const wholeParentMutation = identityPreservingInsertion ||
      followsWholeParentClear ||
      (removed.length > 1 &&
        added.length > 0 &&
        !record.previousSibling &&
        !record.nextSibling &&
        removed.some((element) => overlayChildren.includes(element)));
    if (wholeParentMutation) {
      const identityPreservingRebuild =
        addedOverlaySubsequence.length === overlayChildren.length &&
        addedOverlaySubsequence.every((element, index) =>
          element === overlayChildren[index]
        );
      if (identityPreservingRebuild) {
        // This is a native checkpoint, not a series of moves: every original
        // identity reclaims its own token and interleaved nodes are additions.
        nativeOrderReplaced = false;
        additions.clear();
        movedElements.clear();
        placements.clear();
        replacements.clear();
        ownerByElement.clear();
        elementByOwner.clear();
        for (const element of overlayChildren) {
          assignNativeOwner(element, element);
        }

        let previousOverlay: Element | undefined;
        let additionRun: Element[] = [];
        const flushAdditionRun = (nextOverlay?: Element): void => {
          if (additionRun.length === 0) return;
          const placement: NativeNodePlacementHint = !previousOverlay
            ? { edge: "start", nativeChildGap: 0 }
            : !nextOverlay
            ? { edge: "end", nativeChildGap: overlayChildren.length }
            : {
              before: nextOverlay,
              after: previousOverlay,
              nativeChildGap: overlayIndexByElement.get(nextOverlay),
            };
          for (const element of additionRun) {
            additions.add(element);
            placements.set(element, placement);
          }
          additionRun = [];
        };
        for (const element of added) {
          if (overlaySet.has(element)) {
            flushAdditionRun(element);
            previousOverlay = element;
          } else {
            additionRun.push(element);
          }
        }
        flushAdditionRun();
        continue;
      }

      // A whole-parent identity change establishes a new Firefox-native
      // order. There is no meaningful per-node replacement pairing by index.
      nativeOrderReplaced = true;
      for (const addedElement of added) {
        const owner = removedOwners.get(addedElement);
        if (owner) assignNativeOwner(owner, addedElement);
      }
    }

    const replacementTargets = new Set(replacements.values());
    const replacementSource = wholeParentMutation
      ? undefined
      : removed.length === 1
      ? removed[0]
      : removed.find((element) => !added.includes(element)) ??
        removed.find((element) => replacementTargets.has(element));
    const replacementCount = replacementSource && added.length > 0 ? 1 : 0;
    const recordPlacement = mutationPlacement(
      record,
      overlayIndexByElement,
      overlayChildren.length,
      movedElements,
    );
    let inheritedAdditionPlacement: NativeNodePlacementHint | undefined;
    for (let index = 0; index < replacementCount; index++) {
      const removedElement = replacementSource!;
      const addedElement = added[index];
      if (removedElement === addedElement) {
        additions.add(addedElement);
        if (overlayIndexByElement.has(addedElement)) {
          movedElements.add(addedElement);
        }
        placements.set(addedElement, recordPlacement);
      } else if (additions.delete(removedElement)) {
        // Replacing an item that Firefox added during this transaction still
        // represents an addition relative to the captured native menu.
        additions.add(addedElement);
        if (movedElements.delete(removedElement)) {
          movedElements.add(addedElement);
        }
        inheritedAdditionPlacement = placements.get(removedElement) ??
          recordPlacement;
        placements.delete(removedElement);
        placements.set(addedElement, inheritedAdditionPlacement);
        recordReplacement(removedElement, addedElement);
      } else {
        recordReplacement(removedElement, addedElement);
      }
      const nativeOwner = removedOwners.get(removedElement);
      if (nativeOwner && removedElement !== addedElement) {
        assignNativeOwner(nativeOwner, addedElement);
      }
    }
    for (let index = replacementCount; index < added.length; index++) {
      const addedElement = added[index];
      additions.add(addedElement);
      if (
        overlayIndexByElement.has(addedElement) ||
        replacementTargets.has(addedElement)
      ) {
        movedElements.add(addedElement);
      }
      if (inheritedAdditionPlacement) {
        placements.set(addedElement, inheritedAdditionPlacement);
      } else if (replacementCount > 0) {
        placements.set(addedElement, {
          after: added[replacementCount - 1],
        });
      } else if (wholeParentMutation) {
        placements.set(addedElement, {
          before: added[index + 1],
          after: added[index - 1],
        });
      } else {
        const adjacentAddition = [
          recordPlacement.before,
          recordPlacement.after,
        ].find((element) => element && additions.has(element));
        placements.set(
          addedElement,
          adjacentAddition
            ? placements.get(adjacentAddition) ?? recordPlacement
            : recordPlacement,
        );
      }
    }
    if (added.length === 0) {
      for (const removedElement of removed) {
        promoteAdjacentAdditions(
          removedElement,
          removedOwners.get(removedElement),
        );
      }
    }
  }
  const finalChildren = Array.from(parent.children);
  const finalOverlaySubsequence = finalChildren.filter((element) =>
    overlaySet.has(element)
  );
  const overlayIdentityOrderPreserved =
    finalOverlaySubsequence.length === overlayChildren.length &&
    finalOverlaySubsequence.every((element, index) =>
      element === overlayChildren[index]
    );
  const hasNetWholeParentRebuild = nativeOrderReplaced &&
    !overlayIdentityOrderPreserved;
  if (nativeOrderReplaced && overlayIdentityOrderPreserved) {
    ownerByElement.clear();
    elementByOwner.clear();
    for (const element of overlayChildren) {
      assignNativeOwner(element, element);
      additions.delete(element);
      movedElements.delete(element);
      placements.delete(element);
    }

    let previousOverlay: Element | undefined;
    let additionRun: Element[] = [];
    const flushAdditionRun = (nextOverlay?: Element): void => {
      if (additionRun.length === 0) return;
      const placement: NativeNodePlacementHint = !previousOverlay
        ? { edge: "start", nativeChildGap: 0 }
        : !nextOverlay
        ? { edge: "end", nativeChildGap: overlayChildren.length }
        : {
          before: nextOverlay,
          after: previousOverlay,
          nativeChildGap: overlayIndexByElement.get(nextOverlay),
        };
      for (const element of additionRun) placements.set(element, placement);
      additionRun = [];
    };
    for (const element of finalChildren) {
      if (overlaySet.has(element)) {
        flushAdditionRun(element);
        previousOverlay = element;
      } else if (additions.has(element)) {
        additionRun.push(element);
      }
    }
    flushAdditionRun();
  }
  const survivingAdditions = new Set(
    Array.from(additions).filter((element) =>
      element.parentElement === parent && !ownerByElement.has(element)
    ),
  );
  const survivingMovedElements = new Set(
    Array.from(movedElements).filter((element) =>
      element.parentElement === parent && !ownerByElement.has(element)
    ),
  );
  return {
    additions: survivingAdditions,
    movedElements: survivingMovedElements,
    nativeOwners: new Map(
      Array.from(elementByOwner).filter(([, element]) =>
        element.parentElement === parent
      ),
    ),
    nativeOrderReplaced: hasNetWholeParentRebuild,
    placements: new Map(
      Array.from(placements).filter(([element]) =>
        survivingAdditions.has(element)
      ),
    ),
    replacements,
  };
}

function resolveNativeReplacement(
  original: Element,
  parent: Element,
  replacements: ReadonlyMap<Element, Element>,
  preferReplacement = false,
): Element | null {
  let candidate = preferReplacement
    ? replacements.get(original) ?? original
    : original;
  const seen = new Set<Element>();
  while (candidate.parentElement !== parent) {
    if (seen.has(candidate)) return null;
    seen.add(candidate);
    const replacement = replacements.get(candidate);
    if (!replacement) return null;
    candidate = replacement;
  }
  return candidate;
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
        const mutationRecords: MutationRecord[] = [];
        const reorderRecord: ReorderRecord = {
          parent,
          originalSlots: result.originalSlots.flatMap((slot) => {
            const key = keyByElement.get(slot.element);
            return key ? [{ ...slot, key }] : [];
          }),
          overlayChildren: Array.from(parent.children),
          mutationObserver: null,
          mutationRecords,
        };
        this.#reorders.push(reorderRecord);
        const MutationObserverConstructor = parent.ownerDocument.defaultView
          ?.MutationObserver;
        if (MutationObserverConstructor) {
          reorderRecord.mutationObserver = new MutationObserverConstructor(
            (records) => mutationRecords.push(...records),
          );
          reorderRecord.mutationObserver.observe(parent, { childList: true });
        }
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

    const reorders = this.#reorders.splice(0);

    // Drain and disconnect every observer before touching the DOM. A broken or
    // stale parent must not leave another popup subtree observed indefinitely.
    for (const record of reorders) {
      const observer = record.mutationObserver;
      record.mutationObserver = null;
      if (!observer) continue;
      try {
        record.mutationRecords.push(...observer.takeRecords());
      } catch (error) {
        reportRollbackFailure("observer drain", error);
      } finally {
        try {
          observer.disconnect();
        } catch (error) {
          reportRollbackFailure("observer disconnect", error);
        }
      }
    }

    for (let index = reorders.length - 1; index >= 0; index--) {
      const record = reorders[index];
      try {
        const nativeMutations = summarizeNativeMutations(
          record.mutationRecords,
          record.parent,
          record.overlayChildren,
        );
        if (nativeMutations.nativeOrderReplaced) continue;
        const originalUnmanaged = record.originalSlots[0]?.unmanagedElements ??
          [];
        const movedElements = nativeMutations.movedElements;
        const nativeAdditions = new Set(
          Array.from(nativeMutations.additions).filter((element) =>
            !movedElements.has(element)
          ),
        );
        const unmanagedCandidates = originalUnmanaged.map((original) => {
          const element = nativeMutations.nativeOwners.get(original) ?? null;
          return {
            element,
            nativeIndex: 0,
            original,
            priority: element ? 100 : 0,
          };
        });
        const effectivePlacements = new Map<Element, NativeNodePlacementHint>();
        for (const [element, placement] of nativeMutations.placements) {
          effectivePlacements.set(element, {
            edge: placement.edge,
            nativeChildGap: placement.nativeChildGap,
            before: placement.before
              ? resolveNativeReplacement(
                placement.before,
                record.parent,
                nativeMutations.replacements,
              ) ?? placement.before
              : undefined,
            after: placement.after
              ? resolveNativeReplacement(
                placement.after,
                record.parent,
                nativeMutations.replacements,
              ) ?? placement.after
              : undefined,
          });
        }
        const slotCandidates = record.originalSlots.map((item) => {
          let element = nativeMutations.nativeOwners.get(item.element) ??
            null;
          let priority = element ? 100 : 0;
          if (
            !element && item.element.parentElement !== record.parent &&
            !movedElements.has(item.element)
          ) {
            const replacement = this.#registry.resolveItemForOrdering(
              this.#surface,
              item.key,
            );
            if (
              replacement.status === "resolved" &&
              replacement.element.parentElement === record.parent
            ) {
              element = replacement.element;
              priority = 1;
            }
          }
          return {
            element,
            item,
            nativeIndex: 0,
            priority,
          };
        });
        let nativeIndex = 0;
        for (
          let boundaryCount = 0;
          boundaryCount <= originalUnmanaged.length;
          boundaryCount++
        ) {
          for (const candidate of slotCandidates) {
            if (candidate.item.unmanagedBeforeCount === boundaryCount) {
              candidate.nativeIndex = nativeIndex++;
            }
          }
          if (boundaryCount < originalUnmanaged.length) {
            unmanagedCandidates[boundaryCount].nativeIndex = nativeIndex++;
          }
        }
        type NativeClaim = {
          index: number;
          kind: "managed" | "unmanaged";
          nativeIndex: number;
          priority: number;
        };
        const winnerByElement = new Map<
          Element,
          NativeClaim
        >();
        const claims: Array<NativeClaim & { element: Element }> = [
          ...slotCandidates.flatMap((candidate, index) =>
            candidate.element
              ? [{
                element: candidate.element,
                index,
                kind: "managed" as const,
                nativeIndex: candidate.nativeIndex,
                priority: candidate.priority,
              }]
              : []
          ),
          ...unmanagedCandidates.flatMap((candidate, index) =>
            candidate.element
              ? [{
                element: candidate.element,
                index,
                kind: "unmanaged" as const,
                nativeIndex: candidate.nativeIndex,
                priority: candidate.priority,
              }]
              : []
          ),
        ].sort((first, second) => first.nativeIndex - second.nativeIndex);
        for (const claim of claims) {
          const currentWinner = winnerByElement.get(claim.element);
          if (!currentWinner || claim.priority > currentWinner.priority) {
            winnerByElement.set(claim.element, {
              index: claim.index,
              kind: claim.kind,
              nativeIndex: claim.nativeIndex,
              priority: claim.priority,
            });
          }
        }
        const unmanagedForceMissing = unmanagedCandidates.map(
          (candidate, index) =>
            candidate.element === null ||
            winnerByElement.get(candidate.element)?.kind !== "unmanaged" ||
            winnerByElement.get(candidate.element)?.index !== index,
        );
        const effectiveUnmanaged = unmanagedCandidates.map(
          (candidate, index) =>
            unmanagedForceMissing[index]
              ? candidate.original
              : candidate.element ?? candidate.original,
        );
        const nativeSlots = slotCandidates.map((candidate, index) => {
          const winner = candidate.element
            ? winnerByElement.get(candidate.element)
            : undefined;
          const forceMissing = candidate.element === null
            ? candidate.item.element.parentElement === record.parent &&
              !movedElements.has(candidate.item.element)
            : winner?.kind !== "managed" || winner.index !== index;
          // Keep a missing/reparented slot in the record. The restore policy
          // filters non-survivors itself and needs the original slot count to
          // distinguish a Firefox boundary move from a managed-node removal. A
          // replacement claim wins over the same live element's old native slot
          // regardless of its category or position in native order.
          return {
            element: forceMissing
              ? candidate.item.element
              : candidate.element ?? candidate.item.element,
            forceMissing,
            unmanagedElements: effectiveUnmanaged,
            unmanagedForceMissing,
            unmanagedBeforeCount: candidate.item.unmanagedBeforeCount,
          };
        });
        restoreElementsToNativeSlots(
          record.parent,
          nativeSlots,
          {
            additions: nativeAdditions,
            movedElements,
            placements: effectivePlacements,
          },
        );
      } catch (error) {
        reportRollbackFailure("native-order restore", error);
      }
    }

    const attributes = this.#attributes.splice(0);
    for (let index = attributes.length - 1; index >= 0; index--) {
      const record = attributes[index];
      try {
        record.element.removeAttribute(record.name);
      } catch (error) {
        reportRollbackFailure("attribute cleanup", error);
      }
    }

    const hiddenProperties = this.#hiddenProperties.splice(0);
    for (let index = hiddenProperties.length - 1; index >= 0; index--) {
      const record = hiddenProperties[index];
      try {
        record.element.hidden = record.value;
      } catch (error) {
        reportRollbackFailure("hidden-property cleanup", error);
      }
    }

    const styleLease = this.#styleLease;
    this.#styleLease = null;
    try {
      styleLease?.release();
    } catch (error) {
      reportRollbackFailure("stylesheet release", error);
    }
  }

  private setTransientAttribute(element: Element, name: string): boolean {
    if (element.hasAttribute(name)) return false;
    element.setAttribute(name, "true");
    this.#attributes.push({ element, name });
    return true;
  }
}
