// SPDX-License-Identifier: MPL-2.0

type FirefoxTabRemovalState = {
  closing?: unknown;
  _endRemoveArgs?: unknown;
};

type BrowserWithBrowsingContext = {
  browsingContext?: unknown;
};

type BrowsingContextWithOriginAttributes = {
  originAttributes?: unknown;
};

type OriginAttributesWithUserContextId = {
  userContextId?: unknown;
};

/**
 * Read the container identity from the authoritative browsing context.
 *
 * The `usercontextid` attributes on the tab and browser elements can be
 * present before Firefox has created the replacement browser in that origin
 * context. Never use those mirrored DOM attributes as proof that a native
 * replacement is safe to reuse.
 */
export function getOriginUserContextId(browser: unknown): number | null {
  try {
    if (typeof browser !== "object" || browser === null) {
      return null;
    }

    const browsingContext = (browser as BrowserWithBrowsingContext)
      .browsingContext;
    if (typeof browsingContext !== "object" || browsingContext === null) {
      return null;
    }

    const originAttributes = (
      browsingContext as BrowsingContextWithOriginAttributes
    ).originAttributes;
    if (typeof originAttributes !== "object" || originAttributes === null) {
      return null;
    }

    const userContextId = (
      originAttributes as OriginAttributesWithUserContextId
    ).userContextId;
    return typeof userContextId === "number" &&
        Number.isSafeInteger(userContextId) &&
        userContextId >= 0
      ? userContextId
      : null;
  } catch {
    // Access to linked browser state can fail while Firefox is tearing down a
    // tab. Unknown identity must never authorize replacement reuse.
    return null;
  }
}

/**
 * Fail-closed check for reusing a tracked Firefox replacement in a workspace.
 */
export function hasOriginUserContextId(
  browser: unknown,
  expectedUserContextId: number,
): boolean {
  if (
    !Number.isSafeInteger(expectedUserContextId) ||
    expectedUserContextId < 0
  ) {
    return false;
  }

  return getOriginUserContextId(browser) === expectedUserContextId;
}

/**
 * Firefox sets `_endRemoveArgs[1]` to true before synchronously opening the
 * keep-alive tab that replaces the last visible tab in a window. Treat the
 * private field as a capability: if its shape or value is unavailable, fail
 * safe and do not classify any opened tab as a replacement.
 */
export function hasFirefoxReplacementSignal(tab: unknown): boolean {
  if (typeof tab !== "object" || tab === null) {
    return false;
  }

  const removalState = tab as FirefoxTabRemovalState;
  return removalState.closing === true &&
    Array.isArray(removalState._endRemoveArgs) &&
    removalState._endRemoveArgs[1] === true;
}

/** Remove only the exact Firefox replacement object captured for this close. */
export function excludeTrackedReplacement<T extends object>(
  tabs: readonly T[],
  replacement: T | null,
): T[] {
  return tabs.filter((tab) => tab !== replacement);
}

/**
 * Correlates Firefox's synchronous TabOpen -> TabClose replacement lifecycle.
 * A WeakMap keeps identity authoritative; a WeakSet prevents tabs opened
 * reentrantly by the workspace manager during TabClose from being associated
 * with the closing transaction after it has become terminal.
 */
export class FirefoxTabReplacementTracker<T extends object> {
  private readonly replacementByClosingTab = new WeakMap<T, T>();
  private readonly terminalClosingTabs = new WeakSet<T>();

  observeTabOpen(openedTab: T, tabs: readonly T[]): void {
    const closingCandidates = tabs.filter((candidate) =>
      candidate !== openedTab &&
      !this.terminalClosingTabs.has(candidate) &&
      hasFirefoxReplacementSignal(candidate)
    );

    if (closingCandidates.length !== 1) {
      return;
    }

    const closingTab = closingCandidates[0];
    if (!this.replacementByClosingTab.has(closingTab)) {
      this.replacementByClosingTab.set(closingTab, openedTab);
    }
  }

  /**
   * Make the transaction terminal before its TabClose handler can create tabs,
   * then consume and return the exact TabOpen object captured for it.
   */
  finishTabClose(closingTab: T): T | null {
    this.terminalClosingTabs.add(closingTab);
    const replacement = this.replacementByClosingTab.get(closingTab) ?? null;
    this.replacementByClosingTab.delete(closingTab);
    return replacement;
  }
}
