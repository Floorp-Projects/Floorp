// SPDX-License-Identifier: MPL-2.0

export interface NativeSlotMergeResult {
  changed: boolean;
  originalOrder: Element[];
  originalSlots: NativeSlotRecord[];
}

/** The native region occupied by one managed element before an overlay. */
export interface NativeSlotRecord {
  element: Element;
  forceMissing?: boolean;
  unmanagedElements: readonly Element[];
  unmanagedForceMissing?: readonly boolean[];
  unmanagedBeforeCount: number;
}

export interface NativeNodePlacementHint {
  edge?: "start" | "end";
  before?: Element;
  after?: Element;
  nativeChildGap?: number;
}

export interface NativeRestoreHints {
  additions: ReadonlySet<Element>;
  movedElements: ReadonlySet<Element>;
  placements: ReadonlyMap<Element, NativeNodePlacementHint>;
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

function captureNativeSlots(
  nativeChildren: readonly Element[],
  managed: ReadonlySet<Element>,
): NativeSlotRecord[] {
  const unmanagedElements = nativeChildren.filter((element) =>
    !managed.has(element)
  );
  const unmanagedForceMissing = unmanagedElements.map(() => false);
  const result: NativeSlotRecord[] = [];
  let unmanagedBeforeCount = 0;
  for (const element of nativeChildren) {
    if (managed.has(element)) {
      result.push({
        element,
        unmanagedElements,
        unmanagedForceMissing,
        unmanagedBeforeCount,
      });
    } else {
      unmanagedBeforeCount++;
    }
  }
  return result;
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
    return { changed: false, originalOrder: desired ?? [], originalSlots: [] };
  }

  const managed = new Set(desired);
  const nativeChildren = Array.from(parent.children);
  const originalOrder = nativeChildren.filter((child) => managed.has(child));
  if (originalOrder.length !== desired.length) {
    return { changed: false, originalOrder: [], originalSlots: [] };
  }
  const originalSlots = captureNativeSlots(nativeChildren, managed);
  if (arraysEqual(originalOrder, desired)) {
    return { changed: false, originalOrder, originalSlots };
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
    return { changed: true, originalOrder, originalSlots };
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
    return { changed: false, originalOrder: [], originalSlots: [] };
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
  nativeSlots: readonly NativeSlotRecord[],
  nativeHints: NativeRestoreHints = {
    additions: new Set<Element>(),
    movedElements: new Set<Element>(),
    placements: new Map<Element, NativeNodePlacementHint>(),
  },
): NativeSlotMergeResult {
  const survivingSlots = nativeSlots.filter((slot) =>
    !slot.forceMissing &&
    slot.element.parentElement === parent &&
    !nativeHints.movedElements.has(slot.element)
  );
  if (survivingSlots.length === 0) {
    return { changed: false, originalOrder: [], originalSlots: [] };
  }

  const before = Array.from(parent.children);
  const restorePlaceholders: Comment[] = [];
  try {
    const managed = new Set(survivingSlots.map((slot) => slot.element));
    const unmanagedElements = nativeSlots[0]?.unmanagedElements ?? [];
    const unmanagedForceMissing = nativeSlots[0]?.unmanagedForceMissing ?? [];
    const survivingUnmanaged = unmanagedElements.filter((element, index) =>
      !unmanagedForceMissing[index] && element.parentElement === parent
    );
    const currentSurvivingUnmanaged = before.filter((element) =>
      survivingUnmanaged.includes(element)
    );
    const everyManagedSlotSurvives = survivingSlots.length ===
      nativeSlots.length;
    const nativeBoundaryMoved = survivingUnmanaged.some((element) =>
      nativeHints.movedElements.has(element)
    );
    const unmanagedCrossedManagedSlots = everyManagedSlotSurvives &&
      survivingUnmanaged.some((element) => {
        const originalBoundaryIndex = unmanagedElements.indexOf(element);
        const expectedManagedBefore = survivingSlots.filter((slot) =>
          slot.unmanagedBeforeCount <= originalBoundaryIndex
        ).length;
        const currentBoundaryIndex = before.indexOf(element);
        const currentManagedBefore = before
          .slice(0, currentBoundaryIndex)
          .filter((candidate) =>
            managed.has(candidate)
          ).length;
        return currentManagedBefore !== expectedManagedBefore;
      });
    if (
      !arraysEqual(currentSurvivingUnmanaged, survivingUnmanaged) ||
      nativeBoundaryMoved ||
      unmanagedCrossedManagedSlots
    ) {
      const fallback = mergeElementsIntoNativeSlots(
        parent,
        survivingSlots.map((slot) => slot.element),
      );
      return {
        changed: fallback.changed,
        originalOrder: survivingSlots.map((slot) => slot.element),
        originalSlots: survivingSlots,
      };
    }
    const survivingPrefixCounts = [0];
    for (let index = 0; index < unmanagedElements.length; index++) {
      const element = unmanagedElements[index];
      survivingPrefixCounts.push(
        survivingPrefixCounts.at(-1)! +
          (!unmanagedForceMissing[index] && element.parentElement === parent
            ? 1
            : 0),
      );
    }

    const regionCount = survivingUnmanaged.length + 1;
    const currentForeignByRegion = Array.from(
      { length: regionCount },
      () => [] as Element[],
    );
    let currentRegion = 0;
    for (const element of before) {
      if (element === survivingUnmanaged[currentRegion]) {
        currentRegion++;
      } else if (!managed.has(element)) {
        // Firefox may add or replace conditional menu items while the overlay
        // is active. Keep those native nodes in their current boundary region
        // and weave the restored managed nodes around them below.
        currentForeignByRegion[currentRegion].push(element);
      }
    }

    const desiredSlotsByRegion = Array.from(
      { length: regionCount },
      () => [] as NativeSlotRecord[],
    );
    for (const slot of survivingSlots) {
      const originalBoundaryCount = Math.min(
        slot.unmanagedBeforeCount,
        unmanagedElements.length,
      );
      const region = survivingPrefixCounts[originalBoundaryCount];
      desiredSlotsByRegion[region].push(slot);
    }
    const desiredByRegion = desiredSlotsByRegion.map((slots) =>
      slots.map((slot) => slot.element)
    );
    const nativeIndexBySlot = new Map<NativeSlotRecord, number>();
    const nativeUnmanagedIndices: number[] = [];
    let nativeChildIndex = 0;
    for (
      let boundaryCount = 0;
      boundaryCount <= unmanagedElements.length;
      boundaryCount++
    ) {
      for (const slot of nativeSlots) {
        if (
          Math.min(slot.unmanagedBeforeCount, unmanagedElements.length) ===
            boundaryCount
        ) {
          nativeIndexBySlot.set(slot, nativeChildIndex++);
        }
      }
      if (boundaryCount < unmanagedElements.length) {
        nativeUnmanagedIndices.push(nativeChildIndex++);
      }
    }

    // Reconstruct the original native token stream inside each region. A
    // foreign node in the same region as a missing managed item or unmanaged
    // boundary is most likely Firefox's replacement for that token. Recording
    // its gap among the surviving managed items lets replacement nodes retain
    // their native position without reviving the detached original node.
    const missingNativeGapsByRegion = Array.from(
      { length: regionCount },
      () => [] as number[],
    );
    const movedNativeGapByElement = new Map<
      Element,
      { gap: number; region: number }
    >();
    let nativeRegion = 0;
    let survivingManagedInRegion = 0;
    for (
      let boundaryCount = 0;
      boundaryCount <= unmanagedElements.length;
      boundaryCount++
    ) {
      for (const slot of nativeSlots) {
        if (
          Math.min(slot.unmanagedBeforeCount, unmanagedElements.length) !==
            boundaryCount
        ) {
          continue;
        }
        if (
          !slot.forceMissing &&
          slot.element.parentElement === parent &&
          !nativeHints.movedElements.has(slot.element)
        ) {
          survivingManagedInRegion++;
        } else if (
          !slot.forceMissing && nativeHints.movedElements.has(slot.element)
        ) {
          movedNativeGapByElement.set(slot.element, {
            gap: survivingManagedInRegion,
            region: nativeRegion,
          });
        } else {
          missingNativeGapsByRegion[nativeRegion].push(
            survivingManagedInRegion,
          );
        }
      }

      if (boundaryCount === unmanagedElements.length) break;
      if (
        !unmanagedForceMissing[boundaryCount] &&
        unmanagedElements[boundaryCount].parentElement === parent
      ) {
        nativeRegion++;
        survivingManagedInRegion = 0;
      } else {
        missingNativeGapsByRegion[nativeRegion].push(
          survivingManagedInRegion,
        );
      }
    }

    const currentIndexByElement = new Map(
      before.map((element, index) => [element, index]),
    );
    const desiredSequences = desiredByRegion.map((desired, region) => {
      const gaps = Array.from(
        { length: desired.length + 1 },
        () => [] as Element[],
      );
      let previousGap = 0;
      const foreignElements = currentForeignByRegion[region];
      const replacementGaps = missingNativeGapsByRegion[region];
      let replacementGapIndex = 0;
      for (
        let foreignPosition = 0;
        foreignPosition < foreignElements.length;
        foreignPosition++
      ) {
        const foreign = foreignElements[foreignPosition];
        const foreignIndex = currentIndexByElement.get(foreign) ?? 0;
        const placement = nativeHints.placements.get(foreign);
        const beforePlacementIndex = placement?.before
          ? desired.indexOf(placement.before)
          : -1;
        const afterPlacementIndex = placement?.after
          ? desired.indexOf(placement.after)
          : -1;
        const movedNativeGap = movedNativeGapByElement.get(foreign);
        const placementNativeGap = placement?.nativeChildGap === undefined
          ? null
          : (() => {
            const nativeGap = Math.max(
              0,
              Math.min(placement.nativeChildGap!, nativeChildIndex),
            );
            const originalBoundaryCount = nativeUnmanagedIndices.filter(
              (index) => index < nativeGap,
            ).length;
            const targetRegion = survivingPrefixCounts[originalBoundaryCount];
            if (targetRegion !== region) return null;
            return desiredSlotsByRegion[region].filter((slot) =>
              (nativeIndexBySlot.get(slot) ?? Number.MAX_SAFE_INTEGER) <
                nativeGap
            ).length;
          })();
        let inferredGap: number;
        if (
          !nativeHints.additions.has(foreign) &&
          !nativeHints.movedElements.has(foreign) &&
          replacementGapIndex < replacementGaps.length
        ) {
          inferredGap = replacementGaps[replacementGapIndex++];
        } else if (placementNativeGap !== null) {
          inferredGap = placementNativeGap;
        } else if (placement?.edge === "start") {
          inferredGap = 0;
        } else if (placement?.edge === "end") {
          inferredGap = desired.length;
        } else if (
          beforePlacementIndex >= 0 &&
          afterPlacementIndex >= 0 &&
          beforePlacementIndex !== afterPlacementIndex + 1 &&
          movedNativeGap?.region === region
        ) {
          const beforeDistance = Math.abs(
            beforePlacementIndex - movedNativeGap.gap,
          );
          const afterDistance = Math.abs(
            afterPlacementIndex + 1 - movedNativeGap.gap,
          );
          inferredGap = afterDistance < beforeDistance
            ? afterPlacementIndex + 1
            : beforePlacementIndex;
        } else if (beforePlacementIndex >= 0) {
          inferredGap = beforePlacementIndex;
        } else if (afterPlacementIndex >= 0) {
          inferredGap = afterPlacementIndex + 1;
        } else if (
          placement?.before === survivingUnmanaged[region]
        ) {
          inferredGap = desired.length;
        } else if (
          region > 0 &&
          placement?.after === survivingUnmanaged[region - 1]
        ) {
          inferredGap = 0;
        } else if (foreignIndex === 0) {
          inferredGap = 0;
        } else if (foreignIndex === before.length - 1) {
          inferredGap = desired.length;
        } else if (
          region > 0 &&
          before[foreignIndex - 1] === survivingUnmanaged[region - 1]
        ) {
          inferredGap = 0;
        } else if (
          region < survivingUnmanaged.length &&
          before[foreignIndex + 1] === survivingUnmanaged[region]
        ) {
          inferredGap = desired.length;
        } else {
          let nearestDesiredIndex = -1;
          let nearestCurrentIndex = -1;
          let nearestDistance = Number.POSITIVE_INFINITY;
          for (let index = 0; index < desired.length; index++) {
            const desiredIndex = currentIndexByElement.get(desired[index]);
            if (desiredIndex === undefined) continue;
            const distance = Math.abs(desiredIndex - foreignIndex);
            const preferPrecedingOnTie = distance === nearestDistance &&
              desiredIndex < foreignIndex &&
              nearestCurrentIndex > foreignIndex;
            if (distance < nearestDistance || preferPrecedingOnTie) {
              nearestDesiredIndex = index;
              nearestCurrentIndex = desiredIndex;
              nearestDistance = distance;
            }
          }
          inferredGap = nearestDesiredIndex < 0
            ? desired.length
            : nearestCurrentIndex < foreignIndex
            ? nearestDesiredIndex + 1
            : nearestDesiredIndex;
        }
        // Native nodes must never cross each other even if the customized
        // managed order makes their independently inferred gaps disagree.
        const gap = Math.min(
          desired.length,
          Math.max(previousGap, inferredGap),
        );
        gaps[gap].push(foreign);
        previousGap = gap;
      }

      const sequence: Element[] = [];
      for (let index = 0; index < desired.length; index++) {
        sequence.push(...gaps[index], desired[index]);
      }
      sequence.push(...gaps[desired.length]);
      return sequence;
    });

    for (const element of managed) {
      const placeholder = parent.ownerDocument.createComment(
        `floorp-context-restore-slot:${element.id}`,
      );
      parent.insertBefore(placeholder, element);
      restorePlaceholders.push(placeholder);
      element.remove();
    }

    for (let region = 0; region < regionCount; region++) {
      let insertionReference: Element | null = survivingUnmanaged[region] ??
        null;
      const sequence = desiredSequences[region];
      for (let index = sequence.length - 1; index >= 0; index--) {
        const element = sequence[index];
        if (managed.has(element)) {
          parent.insertBefore(element, insertionReference);
        }
        insertionReference = element;
      }
    }
  } catch (error) {
    console.error("[ContextMenuCustomizer] Native-slot restore failed", error);
    for (const slot of survivingSlots) {
      if (slot.element.parentElement !== parent) {
        parent.appendChild(slot.element);
      }
    }
  } finally {
    for (const placeholder of restorePlaceholders) placeholder.remove();
  }

  const after = Array.from(parent.children);
  return {
    changed: !arraysEqual(before, after),
    originalOrder: survivingSlots.map((slot) => slot.element),
    originalSlots: survivingSlots,
  };
}
