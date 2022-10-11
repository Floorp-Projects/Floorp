/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

async function checkForDeltaMismatch(aMsg) {
  let getDelta = () => {
    return {
      width: this.content.outerWidth - this.content.innerWidth,
      height: this.content.outerHeight - this.content.innerHeight,
    };
  };

  let initialDelta = getDelta();
  let latestDelta = initialDelta;

  this.content.testerPromise = new Promise(resolve => {
    // Called from stopCheck
    this.content.resolveFunc = resolve;
    info(`[${aMsg}] Starting interval tester.`);
    this.content.intervalID = this.content.setInterval(() => {
      let currentDelta = getDelta();
      if (
        latestDelta.width != currentDelta.width ||
        latestDelta.height != currentDelta.height
      ) {
        latestDelta = currentDelta;

        let { innerWidth: iW, outerWidth: oW } = this.content;
        let { innerHeight: iH, outerHeight: oH } = this.content;
        info(`[${aMsg}] Delta changed. (inner ${iW}x${iH}, outer ${oW}x${oH})`);

        let { width: w, height: h } = currentDelta;
        is(w, initialDelta.width, `[${aMsg}] Inner to outer width delta.`);
        is(h, initialDelta.height, `[${aMsg}] Inner to outer height delta.`);
      }
    }, 0);
  }).then(() => {
    let { width: w, height: h } = latestDelta;
    is(w, initialDelta.width, `[${aMsg}] Final inner to outer width delta.`);
    is(h, initialDelta.height, `[${aMsg}] Final Inner to outer height delta.`);
  });
}

async function stopCheck(aMsg) {
  info(`[${aMsg}] Stopping interval tester.`);
  this.content.clearInterval(this.content.intervalID);
  info(`[${aMsg}] Resolving interval tester.`);
  this.content.resolveFunc();
  await this.content.testerPromise;
}

add_task(async function test_innerToOuterDelta() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "https://example.net"
  );
  let popupBrowsingContext = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      info("Opening popup.");
      let popup = this.content.open(
        "https://example.net",
        "",
        "width=200,height=200"
      );
      info("Waiting for load event.");
      await ContentTaskUtils.waitForEvent(popup, "load");
      return popup.browsingContext;
    }
  );

  await SpecialPowers.spawn(
    popupBrowsingContext,
    ["Content"],
    checkForDeltaMismatch
  );
  let popupChrome = popupBrowsingContext.topChromeWindow;
  await SpecialPowers.spawn(popupChrome, ["Chrome"], checkForDeltaMismatch);

  let numResizes = 3;
  let resizeStep = 5;
  let { outerWidth: width, outerHeight: height } = popupChrome;
  let finalWidth = width + numResizes * resizeStep;
  let finalHeight = height + numResizes * resizeStep;

  info(`Starting ${numResizes} resizes.`);
  await new Promise(resolve => {
    let resizeListener = () => {
      if (
        popupChrome.outerWidth == finalWidth &&
        popupChrome.outerHeight == finalHeight
      ) {
        popupChrome.removeEventListener("resize", resizeListener);
        resolve();
      }
    };
    popupChrome.addEventListener("resize", resizeListener);

    let resizeNext = () => {
      width += resizeStep;
      height += resizeStep;
      info(`Resizing to ${width}x${height}`);
      popupChrome.resizeTo(width, height);
      numResizes--;
      if (numResizes > 0) {
        info(`${numResizes} resizes remaining.`);
        popupChrome.requestAnimationFrame(resizeNext);
      }
    };
    resizeNext();
  });

  await SpecialPowers.spawn(popupBrowsingContext, ["Content"], stopCheck);
  await SpecialPowers.spawn(popupChrome, ["Chrome"], stopCheck);

  await SpecialPowers.spawn(popupBrowsingContext, [], () => {
    this.content.close();
  });
  await BrowserTestUtils.removeTab(tab);
});
