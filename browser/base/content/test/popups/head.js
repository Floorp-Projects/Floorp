/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(
  SpecialPowers.Ci.nsIGfxInfo
);

async function waitForBlockedPopups(numberOfPopups, { doc }) {
  let toolbarDoc = doc || document;
  let menupopup = toolbarDoc.getElementById("blockedPopupOptions");
  await BrowserTestUtils.waitForCondition(() => {
    let popups = menupopup.querySelectorAll("[popupReportIndex]");
    return popups.length == numberOfPopups;
  }, `Waiting for ${numberOfPopups} popups`);
}

/*
 * Tests that a sequence of size changes ultimately results in the latest
 * requested size. The test also fails when an unexpected window size is
 * observed in a resize event.
 *
 * aPropertyDeltas    List of objects where keys describe the name of a window
 *                    property and the values the difference to its initial
 *                    value.
 *
 * aInstant           Issue changes without additional waiting in between.
 *
 * A brief example of the resutling code that is effectively run for the
 * following list of deltas:
 * [{outerWidth: 5, outerHeight: 10}, {outerWidth: 10}]
 *
 * let initialWidth = win.outerWidth;
 * let initialHeight = win.outerHeight;
 *
 * if (aInstant) {
 *   win.outerWidth = initialWidth + 5;
 *   win.outerHeight = initialHeight + 10;
 *
 *   win.outerWidth = initialWidth + 10;
 * } else {
 *   win.requestAnimationFrame(() => {
 *     win.outerWidth = initialWidth + 5;
 *     win.outerHeight = initialHeight + 10;
 *
 *     win.requestAnimationFrame(() => {
 *        win.outerWidth = initialWidth + 10;
 *     });
 *   });
 * }
 */
async function testPropertyDeltas(
  aPropertyDeltas,
  aInstant,
  aPropInfo,
  aMsg,
  aWaitForCompletion
) {
  let msg = `[${aMsg}]`;

  let win = this.content.popup || this.content.wrappedJSObject;

  // Property names and mapping from ResizeMoveTest
  let {
    sizeProps,
    positionProps /* can be empty/incomplete as workaround on Linux */,
    readonlyProps,
    crossBoundsMapping,
  } = aPropInfo;

  let stringifyState = state => {
    let stateMsg = sizeProps
      .concat(positionProps)
      .filter(prop => state[prop] !== undefined)
      .map(prop => `${prop}: ${state[prop]}`)
      .join(", ");
    return `{ ${stateMsg} }`;
  };

  let initialState = {};
  let finalState = {};

  info("Initializing all values to current state.");
  for (let prop of sizeProps.concat(positionProps)) {
    let value = win[prop];
    initialState[prop] = value;
    finalState[prop] = value;
  }

  // List of potential states during resize events. The current state is also
  // considered valid, as the resize event might still be outstanding.
  let validResizeStates = [initialState];

  let updateFinalState = (aProp, aDelta) => {
    if (
      readonlyProps.includes(aProp) ||
      !sizeProps.concat(positionProps).includes(aProp)
    ) {
      throw new Error(`Unexpected property "${aProp}".`);
    }

    // Update both properties of the same axis.
    let otherProp = crossBoundsMapping[aProp];
    finalState[aProp] = initialState[aProp] + aDelta;
    finalState[otherProp] = initialState[otherProp] + aDelta;

    // Mark size as valid in resize event.
    if (sizeProps.includes(aProp)) {
      let state = {};
      sizeProps.forEach(p => (state[p] = finalState[p]));
      validResizeStates.push(state);
    }
  };

  info("Adding resize event listener.");
  let resizeCount = 0;
  let resizeListener = evt => {
    resizeCount++;

    let currentSizeState = {};
    sizeProps.forEach(p => (currentSizeState[p] = win[p]));

    info(
      `${msg} ${resizeCount}. resize event: ${stringifyState(currentSizeState)}`
    );
    let matchingIndex = validResizeStates.findIndex(state =>
      sizeProps.every(p => state[p] == currentSizeState[p])
    );
    if (matchingIndex < 0) {
      info(`${msg} Size state should have been one of:`);
      for (let state of validResizeStates) {
        info(stringifyState(state));
      }
    }

    if (win.gBrowser && evt.target != win) {
      // Without e10s we receive content resize events in chrome windows.
      todo(false, `${msg} Resize event target is our window.`);
      return;
    }

    Assert.greaterOrEqual(
      matchingIndex,
      0,
      `${msg} Valid intermediate state. Current: ` +
        stringifyState(currentSizeState)
    );

    // No longer allow current and preceding states.
    validResizeStates.splice(0, matchingIndex + 1);
  };
  win.addEventListener("resize", resizeListener);

  info("Starting property changes.");
  await new Promise(resolve => {
    let index = 0;
    let next = async () => {
      let pre = `${msg} [${index + 1}/${aPropertyDeltas.length}]`;

      let deltaObj = aPropertyDeltas[index];
      for (let prop in deltaObj) {
        updateFinalState(prop, deltaObj[prop]);

        let targetValue = initialState[prop] + deltaObj[prop];
        info(`${pre} Setting ${prop} to ${targetValue}.`);
        if (sizeProps.includes(prop)) {
          win.resizeTo(finalState.outerWidth, finalState.outerHeight);
        } else {
          win.moveTo(finalState.screenX, finalState.screenY);
        }
        if (aWaitForCompletion) {
          await ContentTaskUtils.waitForCondition(
            () => win[prop] == targetValue,
            `${msg} Waiting for ${prop} to be ${targetValue}.`
          );
        }
      }

      index++;
      if (index < aPropertyDeltas.length) {
        scheduleNext();
      } else {
        resolve();
      }
    };

    let scheduleNext = () => {
      if (aInstant) {
        next();
      } else {
        info(`${msg} Requesting animation frame.`);
        win.requestAnimationFrame(next);
      }
    };
    scheduleNext();
  });

  try {
    info(`${msg} Waiting for window to match the final state.`);
    await ContentTaskUtils.waitForCondition(
      () => sizeProps.concat(positionProps).every(p => win[p] == finalState[p]),
      "Waiting for final state."
    );
  } catch (e) {}

  info(`${msg} Checking final state.`);
  info(`${msg} Exepected: ${stringifyState(finalState)}`);
  info(`${msg} Actual:    ${stringifyState(win)}`);
  for (let prop of sizeProps.concat(positionProps)) {
    is(win[prop], finalState[prop], `${msg} Expected final value for ${prop}`);
  }

  win.removeEventListener("resize", resizeListener);
}

function roundedCenter(aDimension, aOrigin) {
  let center = aOrigin + Math.floor(aDimension / 2);
  return center - (center % 100);
}

class ResizeMoveTest {
  static WindowWidth = 200;
  static WindowHeight = 200;
  static WindowLeft = roundedCenter(screen.availWidth - 200, screen.left);
  static WindowTop = roundedCenter(screen.availHeight - 200, screen.top);

  static PropInfo = {
    sizeProps: ["outerWidth", "outerHeight", "innerWidth", "innerHeight"],
    positionProps: [
      "screenX",
      "screenY",
      /* readonly */ "mozInnerScreenX",
      /* readonly */ "mozInnerScreenY",
    ],
    readonlyProps: ["mozInnerScreenX", "mozInnerScreenY"],
    crossAxisMapping: {
      outerWidth: "outerHeight",
      outerHeight: "outerWidth",
      innerWidth: "innerHeight",
      innerHeight: "innerWidth",
      screenX: "screenY",
      screenY: "screenX",
      mozInnerScreenX: "mozInnerScreenY",
      mozInnerScreenY: "mozInnerScreenX",
    },
    crossBoundsMapping: {
      outerWidth: "innerWidth",
      outerHeight: "innerHeight",
      innerWidth: "outerWidth",
      innerHeight: "outerHeight",
      screenX: "mozInnerScreenX",
      screenY: "mozInnerScreenY",
      mozInnerScreenX: "screenX",
      mozInnerScreenY: "screenY",
    },
  };

  constructor(
    aPropertyDeltas,
    aInstant = false,
    aMsg = "ResizeMoveTest",
    aWaitForCompletion = false
  ) {
    this.propertyDeltas = aPropertyDeltas;
    this.instant = aInstant;
    this.msg = aMsg;
    this.waitForCompletion = aWaitForCompletion;

    // Allows to ignore positions while testing.
    this.ignorePositions = false;
    // Allows to ignore only mozInnerScreenX/Y properties while testing.
    this.ignoreMozInnerScreen = false;
    // Allows to skip checking the restored position after testing.
    this.ignoreRestoredPosition = false;

    if (AppConstants.platform == "linux" && !SpecialPowers.isHeadless) {
      // We can occasionally start the test while nsWindow reports a wrong
      // client offset (gdk origin and root_origin are out of sync). This
      // results in false expectations for the final mozInnerScreenX/Y values.
      this.ignoreMozInnerScreen = !ResizeMoveTest.hasCleanUpTask;

      let { positionProps } = ResizeMoveTest.PropInfo;
      let resizeOnlyTest = aPropertyDeltas.every(deltaObj =>
        positionProps.every(prop => deltaObj[prop] === undefined)
      );

      let isWayland = gfxInfo.windowProtocol == "wayland";
      if (resizeOnlyTest && isWayland) {
        // On Wayland we can't move the window in general. The window also
        // doesn't necessarily open our specified position.
        this.ignoreRestoredPosition = true;
        // We can catch bad screenX/Y at the start of the first test in a
        // window.
        this.ignorePositions = !ResizeMoveTest.hasCleanUpTask;
      }
    }

    if (!ResizeMoveTest.hasCleanUpTask) {
      ResizeMoveTest.hasCleanUpTask = true;
      registerCleanupFunction(ResizeMoveTest.Cleanup);
    }

    add_task(async () => {
      let tab = await ResizeMoveTest.GetOrCreateTab();
      let browsingContext =
        await ResizeMoveTest.GetOrCreatePopupBrowsingContext();
      if (!browsingContext) {
        return;
      }

      info("=== Running in content. ===");
      await this.run(browsingContext, `${this.msg} (content)`);
      await this.restorePopupState(browsingContext);

      info("=== Running in chrome. ===");
      let popupChrome = browsingContext.topChromeWindow;
      await this.run(popupChrome.browsingContext, `${this.msg} (chrome)`);
      await this.restorePopupState(browsingContext);

      info("=== Running in opener. ===");
      await this.run(tab.linkedBrowser, `${this.msg} (opener)`);
      await this.restorePopupState(browsingContext);
    });
  }

  async run(aBrowsingContext, aMsg) {
    let testType = this.instant ? "instant" : "fanned out";
    let msg = `${aMsg} (${testType})`;

    let propInfo = {};
    for (let k in ResizeMoveTest.PropInfo) {
      propInfo[k] = ResizeMoveTest.PropInfo[k];
    }
    if (this.ignoreMozInnerScreen) {
      todo(false, `[${aMsg}] Shouldn't ignore mozInnerScreenX/Y.`);
      propInfo.positionProps = propInfo.positionProps.filter(
        prop => !["mozInnerScreenX", "mozInnerScreenY"].includes(prop)
      );
    }
    if (this.ignorePositions) {
      todo(false, `[${aMsg}] Shouldn't ignore position.`);
      propInfo.positionProps = [];
    }

    info(`${msg}: ` + JSON.stringify(this.propertyDeltas));
    await SpecialPowers.spawn(
      aBrowsingContext,
      [
        this.propertyDeltas,
        this.instant,
        propInfo,
        msg,
        this.waitForCompletion,
      ],
      testPropertyDeltas
    );
  }

  async restorePopupState(aBrowsingContext) {
    info("Restore popup state.");

    let { deltaWidth, deltaHeight } = await SpecialPowers.spawn(
      aBrowsingContext,
      [],
      () => {
        return {
          deltaWidth: this.content.outerWidth - this.content.innerWidth,
          deltaHeight: this.content.outerHeight - this.content.innerHeight,
        };
      }
    );

    let chromeWindow = aBrowsingContext.topChromeWindow;
    let {
      WindowLeft: left,
      WindowTop: top,
      WindowWidth: width,
      WindowHeight: height,
    } = ResizeMoveTest;

    chromeWindow.resizeTo(width + deltaWidth, height + deltaHeight);
    chromeWindow.moveTo(left, top);

    await SpecialPowers.spawn(
      aBrowsingContext,
      [left, top, width, height, this.ignoreRestoredPosition],
      async (aLeft, aTop, aWidth, aHeight, aIgnorePosition) => {
        let win = this.content.wrappedJSObject;

        info("Waiting for restored size.");
        await ContentTaskUtils.waitForCondition(
          () => win.innerWidth == aWidth && win.innerHeight === aHeight,
          "Waiting for restored size."
        );
        is(win.innerWidth, aWidth, "Restored width.");
        is(win.innerHeight, aHeight, "Restored height.");

        if (!aIgnorePosition) {
          info("Waiting for restored position.");
          await ContentTaskUtils.waitForCondition(
            () => win.screenX == aLeft && win.screenY === aTop,
            "Waiting for restored position."
          );
          is(win.screenX, aLeft, "Restored screenX.");
          is(win.screenY, aTop, "Restored screenY.");
        } else {
          todo(false, "Shouldn't ignore restored position.");
        }
      }
    );
  }

  static async GetOrCreateTab() {
    if (ResizeMoveTest.tab) {
      return ResizeMoveTest.tab;
    }

    info("Opening tab.");
    ResizeMoveTest.tab = await BrowserTestUtils.openNewForegroundTab(
      window.gBrowser,
      "https://example.net/browser/browser/base/content/test/popups/popup_blocker_a.html"
    );
    return ResizeMoveTest.tab;
  }

  static async GetOrCreatePopupBrowsingContext() {
    if (ResizeMoveTest.popupBrowsingContext) {
      if (!ResizeMoveTest.popupBrowsingContext.isActive) {
        return undefined;
      }
      return ResizeMoveTest.popupBrowsingContext;
    }

    let tab = await ResizeMoveTest.GetOrCreateTab();
    info("Opening popup.");
    ResizeMoveTest.popupBrowsingContext = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [
        ResizeMoveTest.WindowWidth,
        ResizeMoveTest.WindowHeight,
        ResizeMoveTest.WindowLeft,
        ResizeMoveTest.WindowTop,
      ],
      async (aWidth, aHeight, aLeft, aTop) => {
        let win = this.content.open(
          this.content.document.location.href,
          "_blank",
          `left=${aLeft},top=${aTop},width=${aWidth},height=${aHeight}`
        );
        this.content.popup = win;

        await new Promise(r => (win.onload = r));

        return win.browsingContext;
      }
    );

    return ResizeMoveTest.popupBrowsingContext;
  }

  static async Cleanup() {
    let browsingContext = ResizeMoveTest.popupBrowsingContext;
    if (browsingContext) {
      await SpecialPowers.spawn(browsingContext, [], () => {
        this.content.close();
      });
      delete ResizeMoveTest.popupBrowsingContext;
    }

    let tab = ResizeMoveTest.tab;
    if (tab) {
      await BrowserTestUtils.removeTab(tab);
      delete ResizeMoveTest.tab;
    }
    ResizeMoveTest.hasCleanUpTask = false;
  }
}

function chaosRequestLongerTimeout(aDoRequest) {
  if (aDoRequest && parseInt(Services.env.get("MOZ_CHAOSMODE"), 16)) {
    requestLongerTimeout(2);
  }
}

function createGenericResizeTests(aFirstValue, aSecondValue, aInstant, aMsg) {
  // Runtime almost doubles in chaos mode on Mac.
  chaosRequestLongerTimeout(AppConstants.platform == "macosx");

  let { crossBoundsMapping, crossAxisMapping } = ResizeMoveTest.PropInfo;

  for (let prop of ["innerWidth", "outerHeight"]) {
    // Mixing inner and outer property.
    for (let secondProp of [prop, crossBoundsMapping[prop]]) {
      let first = {};
      first[prop] = aFirstValue;
      let second = {};
      second[secondProp] = aSecondValue;
      new ResizeMoveTest(
        [first, second],
        aInstant,
        `${aMsg} ${prop},${secondProp}`
      );
    }
  }

  for (let prop of ["innerHeight", "outerWidth"]) {
    let first = {};
    first[prop] = aFirstValue;
    let second = {};
    second[prop] = aSecondValue;

    // Setting property of other axis before/between two changes.
    let otherProps = [
      crossAxisMapping[prop],
      crossAxisMapping[crossBoundsMapping[prop]],
    ];
    for (let interferenceProp of otherProps) {
      let interference = {};
      interference[interferenceProp] = 20;
      new ResizeMoveTest(
        [first, interference, second],
        aInstant,
        `${aMsg} ${prop},${interferenceProp},${prop}`
      );
      new ResizeMoveTest(
        [interference, first, second],
        aInstant,
        `${aMsg} ${interferenceProp},${prop},${prop}`
      );
    }
  }
}

function createGenericMoveTests(aInstant, aMsg) {
  // Runtime almost doubles in chaos mode on Mac.
  chaosRequestLongerTimeout(AppConstants.platform == "macosx");

  let { crossAxisMapping } = ResizeMoveTest.PropInfo;

  for (let prop of ["screenX", "screenY"]) {
    for (let [v1, v2, msg] of [
      [9, 10, `${aMsg}`],
      [11, 11, `${aMsg} repeat`],
      [12, 0, `${aMsg} revert`],
    ]) {
      let first = {};
      first[prop] = v1;
      let second = {};
      second[prop] = v2;
      new ResizeMoveTest([first, second], aInstant, `${msg} ${prop},${prop}`);

      let interferenceProp = crossAxisMapping[prop];
      let interference = {};
      interference[interferenceProp] = 20;
      new ResizeMoveTest(
        [first, interference, second],
        aInstant,
        `${aMsg} ${prop},${interferenceProp},${prop}`
      );
      new ResizeMoveTest(
        [interference, first, second],
        aInstant,
        `${msg} ${interferenceProp},${prop},${prop}`
      );
    }
  }
}
