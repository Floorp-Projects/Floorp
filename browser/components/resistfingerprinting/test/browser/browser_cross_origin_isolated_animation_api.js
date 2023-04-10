/**
 * Bug 1621677 - A test for making sure getting the correct (higher) precision
 *   when it's cross-origin-isolated on animation APIs.
 */

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the test case for window in
// test_animation_api.html. The main difference is this test case
// verifies animation has more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
add_task(async function runRTPTestAnimation() {
  let runTests = async function(data) {
    function waitForCondition(aCond, aCallback, aErrorMsg) {
      var tries = 0;
      var interval = content.setInterval(() => {
        if (tries >= 30) {
          ok(false, aErrorMsg);
          moveOn();
          return;
        }
        var conditionPassed;
        try {
          conditionPassed = aCond();
        } catch (e) {
          ok(false, `${e}\n${e.stack}`);
          conditionPassed = false;
        }
        if (conditionPassed) {
          moveOn();
        }
        tries++;
      }, 100);
      var moveOn = () => {
        content.clearInterval(interval);
        aCallback();
      };
    }

    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    const testDiv = content.document.getElementById("testDiv");
    const animation = testDiv.animate({ opacity: [0, 1] }, 100000);
    animation.play();

    let done;
    let promise = new Promise(resolve => {
      done = resolve;
    });

    waitForCondition(
      () => animation.currentTime > 100,
      () => {
        // We have disabled Time Precision Reduction for CSS Animations, so we
        // expect those tests to fail.
        // If we are testing that preference, we accept either rounded or not
        // rounded values as A-OK.
        var maybeAcceptEverything = function(value) {
          if (data.reduceTimerPrecision && !data.resistFingerprinting) {
            return true;
          }

          return value;
        };

        ok(
          maybeAcceptEverything(
            isRounded(animation.startTime, expectedPrecision)
          ),
          `Animation.startTime with precision ${expectedPrecision} is not ` +
            `rounded: ${animation.startTime}`
        );
        ok(
          maybeAcceptEverything(
            isRounded(animation.currentTime, expectedPrecision)
          ),
          `Animation.currentTime with precision ${expectedPrecision} is ` +
            `not rounded: ${animation.currentTime}`
        );
        ok(
          maybeAcceptEverything(
            isRounded(animation.timeline.currentTime, expectedPrecision)
          ),
          `Animation.timeline.currentTime with precision ` +
            `${expectedPrecision} is not rounded: ` +
            `${animation.timeline.currentTime}`
        );
        if (content.document.timeline) {
          ok(
            maybeAcceptEverything(
              isRounded(
                content.document.timeline.currentTime,
                expectedPrecision
              )
            ),
            `Document.timeline.currentTime with precision ` +
              `${expectedPrecision} is not rounded: ` +
              `${content.document.timeline.currentTime}`
          );
        }
        done();
      },
      "animation failed to start"
    );

    await promise;
  };

  await setupAndRunCrossOriginIsolatedTest(true, true, true, 100, runTests);
  await setupAndRunCrossOriginIsolatedTest(true, false, true, 50, runTests);
  await setupAndRunCrossOriginIsolatedTest(true, false, true, 0.1, runTests);
  await setupAndRunCrossOriginIsolatedTest(true, true, true, 0.013, runTests);

  await setupAndRunCrossOriginIsolatedTest(false, true, true, 0.005, runTests);
});
