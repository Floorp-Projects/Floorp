/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import type {
  DroppedRun,
  GuardedScrollbox,
  StreamAxis,
  TabBrowserLike,
  WheelGuardReadout,
} from "./types.ts";

/**
 * Stops the overflowing tab strip "snapping back" after a trackpad scroll.
 *
 * The snap-back is delivered as real wheel events: after the fingers lift,
 * macOS emits a decaying momentum tail whose axis mix follows the lift
 * velocity, not the drag — so a horizontal strip-scroll can hand over to a
 * vertical-dominant (or reversed) tail. Stock arrowscrollbox maps deltaY
 * onto horizontal scrolling ("vertical wheel scrolls a horizontal
 * scrollbox") behind only a 2-event axis filter, so from the tail's third
 * event the strip scrolls back against the drag it just finished.
 *
 * Two rules, capture-phase, ahead of every strip wheel listener; pixel-mode
 * (trackpad) events only, so plain mouse wheels never see the guard:
 *  - axis quarantine: inside one gapless stream, vertical-dominant events
 *    may not continue a horizontal-dominant stream. A deliberate vertical
 *    strip-scroll starts its own stream (>400ms gap) and still works.
 *  - reversal hold: a direction reversal inside a gapless same-axis stream
 *    is held while it stays under the run's peak envelope. A momentum tail
 *    enters at its peak and decays — frame jitter (…13, 12, 15, 11…) can
 *    exceed the PREVIOUS event but never climbs back above the entry peak,
 *    so the release must compare against the run peak, not the last event
 *    (a last-event comparison leaked real tails). A live finger enters
 *    near zero and clears its own tiny peak within an event or two.
 *
 * A third rule covers the non-wheel half of the symptom: Firefox re-runs
 * ensureTabIsVisible(selectedTab) on every strip resize (tabs.js
 * handleResize -> _handleTabSelect), and strip resizes trail manual
 * scrolling with a delay (lazy tab rendering settling, scroll-button and
 * stack-bar churn) — yanking the strip back to the selected tab seconds
 * after the user scrolled away from it:
 *  - recentre latch: a manual wheel over the strip marks it
 *    user-positioned for the CURRENT selected tab; while the selection is
 *    unchanged, resize-driven recentres onto that tab are skipped.
 *    Selecting any tab (the only recentre the user actually asked for)
 *    ends the suppression by identity, so no listener ordering is
 *    involved.
 *
 * Pref (int) floorp.tabstrip.wheelguard — bitfield: 1 axis quarantine |
 * 2 reversal hold | 4 recentre latch; default 7 (all). Flip to 0 for
 * stock behavior. Readout at globalThis.__floorpWheelGuard says which
 * rules engaged.
 */

const GUARD_PREF = "floorp.tabstrip.wheelguard";
const AXIS_RULE = 1;
const REVERSAL_RULE = 2;
const RECENTRE_RULE = 4;
const GUARD_DEFAULT = AXIS_RULE | REVERSAL_RULE | RECENTRE_RULE;

/** A pause longer than this starts a fresh stream (re-latches axis and
 * direction). Momentum tails run gapless at display rate and the
 * touch-to-momentum handoff pause stays well under this. */
const STREAM_GAP_MS = 400;

/** A held reversal must clear runPeak * factor + 1 to count as a live
 * finger instead of a decaying tail's jitter. */
const PEAK_FACTOR = 1.05;

const READOUT_KEY = "__floorpWheelGuard";

@noraComponent(import.meta.hot)
export default class TabStripWheelGuard extends NoraComponentBase {
  init(): void {
    const tabContainer = document?.getElementById("tabbrowser-tabs");
    const asb = document?.getElementById(
      "tabbrowser-arrowscrollbox",
    ) as GuardedScrollbox | null;
    if (!tabContainer || !asb) {
      this.logger.warn("tab strip not found; guard not attached");
      return;
    }

    const readout: WheelGuardReadout = {
      axisDropped: 0,
      reversalDropped: 0,
      recentresSuppressed: 0,
      lastRun: null,
      reset() {
        this.axisDropped = 0;
        this.reversalDropped = 0;
        this.recentresSuppressed = 0;
        this.lastRun = null;
      },
    };
    (globalThis as Record<string, unknown> & typeof globalThis)[READOUT_KEY] =
      readout;

    const getSelectedTab = (): Element | null =>
      (globalThis as { gBrowser?: TabBrowserLike }).gBrowser?.selectedTab ??
        null;

    let lastTs = 0;
    let streamAxis: StreamAxis | null = null;
    let streamDir = 0;
    let runPeak = -1;
    let run: DroppedRun | null = null;
    let errorReported = false;
    // The tab whose resize-driven recentres are suppressed because the
    // user manually positioned the strip while it was selected. Cleared
    // by identity: any selection change makes it stale.
    let userPositionedFor: Element | null = null;

    const drop = (
      event: WheelEvent,
      axis: StreamAxis,
      px: number,
    ): void => {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (!run) {
        run = { axis, events: 0, px: 0, endedAt: 0 };
        readout.lastRun = run;
      }
      run.events += 1;
      run.px += px;
      run.endedAt = Date.now();
    };

    const onWheelCapture = (event: WheelEvent): void => {
      try {
        const mode = Services.prefs.getIntPref(GUARD_PREF, GUARD_DEFAULT);
        if (mode <= 0) return;
        if (!(event.target instanceof Node) || !asb.contains(event.target)) {
          return;
        }
        // Any manual wheel over the strip — pixel or line mode — is the
        // user positioning it; arm the recentre latch for the currently
        // selected tab.
        userPositionedFor = getSelectedTab();
        // Trackpads deliver pixel-mode streams; line/page devices have no
        // momentum machinery and pass untouched.
        if (event.deltaMode !== event.DOM_DELTA_PIXEL) return;
        // Enforcement mirrors stock on_wheel's bail-outs — but the stream
        // model must keep OBSERVING while enforcement is off (e.g. the
        // strip briefly loses [overflowing] during session restore or tab
        // churn). If observation paused too, a momentum tail arriving as
        // the first event after a blind window would latch as a fresh
        // gesture and pass wholesale.
        const enforcing = asb.hasAttribute("overflowing") &&
          asb.getAttribute("orient") !== "vertical";

        const absX = Math.abs(event.deltaX);
        const absY = Math.abs(event.deltaY);
        if (absX === 0 && absY === 0) return;

        // Same delta selection stock on_wheel performs.
        const isVertical = absY > absX;
        const rawDelta = isVertical ? event.deltaY : event.deltaX;
        const effDelta = isVertical && asb.isRTLScrollbox
          ? -rawDelta
          : rawDelta;
        const axis: StreamAxis = isVertical ? "v" : "h";
        const dir = effDelta < 0 ? -1 : 1;
        const mag = Math.abs(effDelta);

        const gap = event.timeStamp - lastTs;
        lastTs = event.timeStamp;

        // Fresh stream, or observe-only (a blind event reaches stock and
        // becomes the stream's reality — track it, never drop it).
        if (streamAxis === null || gap > STREAM_GAP_MS || !enforcing) {
          streamAxis = axis;
          streamDir = dir;
          runPeak = -1;
          run = null;
          return;
        }

        if (axis !== streamAxis) {
          if (streamAxis === "h" && (mode & AXIS_RULE) !== 0) {
            readout.axisDropped += 1;
            drop(event, axis, mag);
            return;
          }
          // Horizontal is the strip's native axis — a stream turning
          // horizontal-dominant is the user, let it re-latch.
          streamAxis = axis;
          streamDir = dir;
          runPeak = -1;
          run = null;
          return;
        }

        if (dir === streamDir) {
          runPeak = -1;
          run = null;
          return;
        }

        if ((mode & REVERSAL_RULE) !== 0) {
          if (runPeak >= 0 && mag > runPeak * PEAK_FACTOR + 1) {
            streamDir = dir;
            runPeak = -1;
            run = null;
            return;
          }
          runPeak = Math.max(runPeak, mag);
          readout.reversalDropped += 1;
          drop(event, axis, mag);
          return;
        }

        streamDir = dir;
        runPeak = -1;
      } catch (e) {
        if (!errorReported) {
          errorReported = true;
          console.error("[TabStripWheelGuard]", e);
        }
      }
    };

    tabContainer.addEventListener("wheel", onWheelCapture, {
      capture: true,
      passive: false,
    });

    // Recentre latch: intercept Firefox's resize-driven scroll-to-selected
    // (tabs.js handleResize -> _handleTabSelect -> ensureTabIsVisible).
    // Composes with tab-stacks' hidden-tab wrap — each skips on its own
    // condition and calls through otherwise.
    if (!asb.__floorpRecentreWrapped) {
      const origEnsure = asb.ensureElementIsVisible.bind(asb);
      asb.ensureElementIsVisible = (el: Element, instant?: boolean) => {
        try {
          const mode = Services.prefs.getIntPref(GUARD_PREF, GUARD_DEFAULT);
          if ((mode & RECENTRE_RULE) !== 0 && userPositionedFor) {
            const selected = getSelectedTab();
            if (selected !== userPositionedFor) {
              // Selection changed since the user positioned the strip —
              // suppression is over, recentres are legitimate again.
              userPositionedFor = null;
            } else if (el === selected) {
              readout.recentresSuppressed += 1;
              return;
            }
          }
        } catch (e) {
          if (!errorReported) {
            errorReported = true;
            console.error("[TabStripWheelGuard]", e);
          }
        }
        return origEnsure(el, instant);
      };
      asb.__floorpRecentreWrapped = true;
      onCleanup(() => {
        asb.ensureElementIsVisible = origEnsure;
        asb.__floorpRecentreWrapped = false;
      });
    }
    this.logger.debug("attached");

    onCleanup(() => {
      tabContainer.removeEventListener("wheel", onWheelCapture, true);
      delete (globalThis as Record<string, unknown> & typeof globalThis)[
        READOUT_KEY
      ];
    });
  }
}
