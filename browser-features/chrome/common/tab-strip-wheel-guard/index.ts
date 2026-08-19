// SPDX-License-Identifier: MPL-2.0

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { onCleanup } from "solid-js";
import { classifyWheelEvent, emptyWheelGuardState } from "./classifier.ts";
import {
  type InstalledWheelGuard,
  TAB_STRIP_WHEEL_GUARD_PREF,
  type WheelGuardEnvironment,
  type WheelGuardGlobalObject,
  type WheelGuardReadout,
  WHEEL_GUARD_SUPPORTED_MASK,
} from "./types.ts";

interface NativeArrowScrollbox extends Element {
  readonly overflowing?: boolean;
  readonly isRTLScrollbox?: boolean;
}

interface NativeTabContainer extends EventTarget {
  readonly verticalMode?: boolean;
  readonly arrowScrollbox?: NativeArrowScrollbox;
}

const WHEEL_LISTENER_OPTIONS: AddEventListenerOptions = {
  capture: true,
  passive: false,
};

export function installWheelGuard(
  prefValue: number,
  environment: WheelGuardEnvironment,
): InstalledWheelGuard | null {
  const mode = prefValue & WHEEL_GUARD_SUPPORTED_MASK;
  if (mode === 0) {
    return null;
  }

  let state = emptyWheelGuardState();
  let destroyed = false;
  const readout: WheelGuardReadout = {
    mode,
    unsupportedBits: prefValue & ~WHEEL_GUARD_SUPPORTED_MASK,
    axisDropped: 0,
    reversalDropped: 0,
    passed: 0,
    ignored: 0,
    lastDecision: null,
    reset: () => {
      state = emptyWheelGuardState();
      readout.axisDropped = 0;
      readout.reversalDropped = 0;
      readout.passed = 0;
      readout.ignored = 0;
      readout.lastDecision = null;
    },
  };

  const onWheel = (event: Event): void => {
    if (!(event instanceof WheelEvent)) {
      return;
    }
    const classification = classifyWheelEvent(
      {
        mode,
        deltaMode: event.deltaMode,
        deltaX: event.deltaX,
        deltaY: event.deltaY,
        timestamp: environment.timestampFor(event),
        overflowing: environment.isOverflowing(),
        verticalTabStrip: environment.isVerticalTabStrip(),
        rtl: environment.isRtl(),
      },
      state,
    );
    state = classification.state;
    readout.lastDecision = classification.decision;

    if (classification.outcome === "ignore") {
      readout.ignored += 1;
      return;
    }
    if (classification.outcome === "pass") {
      readout.passed += 1;
      return;
    }

    if (classification.decision === "dropped-axis") {
      readout.axisDropped += 1;
    } else {
      readout.reversalDropped += 1;
    }
    event.stopPropagation();
    event.preventDefault();
  };

  environment.target.addEventListener(
    "wheel",
    onWheel,
    WHEEL_LISTENER_OPTIONS,
  );
  environment.globalObject.__floorpWheelGuard = readout;

  return {
    readout,
    destroy: () => {
      if (destroyed) {
        return;
      }
      destroyed = true;
      environment.target.removeEventListener(
        "wheel",
        onWheel,
        WHEEL_LISTENER_OPTIONS,
      );
      if (environment.globalObject.__floorpWheelGuard === readout) {
        delete environment.globalObject.__floorpWheelGuard;
      }
      state = emptyWheelGuardState();
    },
  };
}

@noraComponent(import.meta.hot)
export default class TabStripWheelGuard extends NoraComponentBase {
  init(): void {
    let prefValue = 0;
    try {
      prefValue = Services.prefs.getIntPref(TAB_STRIP_WHEEL_GUARD_PREF, 0);
    } catch {
      return;
    }
    if ((prefValue & WHEEL_GUARD_SUPPORTED_MASK) === 0) {
      return;
    }

    const tabContainer = globalThis.gBrowser
      ?.tabContainer as NativeTabContainer | undefined;
    if (!tabContainer) {
      return;
    }
    const globalObject = globalThis as unknown as WheelGuardGlobalObject;
    const installed = installWheelGuard(prefValue, {
      target: tabContainer,
      globalObject,
      isOverflowing: () => Boolean(tabContainer.arrowScrollbox?.overflowing),
      isVerticalTabStrip: () => Boolean(tabContainer.verticalMode),
      isRtl: () => Boolean(tabContainer.arrowScrollbox?.isRTLScrollbox),
      timestampFor: (event) => event.timeStamp,
    });
    if (!installed) {
      return;
    }
    onCleanup(() => installed.destroy());
  }
}
