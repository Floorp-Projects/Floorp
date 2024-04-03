/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_selectingElementsInIframes() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: IFRAME_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      helper.triggerUIFromToolbar();

      // There are two iframes in the test page. One iframe is nested in the
      // other so we SpecialPowers.spawn into the iframes to get the
      // dimension/position of the elements within each iframe.
      let elementDimensions = await SpecialPowers.spawn(
        browser,
        [],
        async () => {
          let divDims = content.document
            .querySelector("div")
            .getBoundingClientRect();

          let iframe = content.document.querySelector("iframe");
          let iframesDivsDimArr = await SpecialPowers.spawn(
            iframe,
            [],
            async () => {
              let iframeDivDims = content.document
                .querySelector("div")
                .getBoundingClientRect();

              // Element within the first iframe
              iframeDivDims = {
                left: iframeDivDims.left + content.window.mozInnerScreenX,
                top: iframeDivDims.top + content.window.mozInnerScreenY,
                width: iframeDivDims.width,
                height: iframeDivDims.height,
              };

              let nestedIframe = content.document.querySelector("iframe");
              let nestedIframeDivDims = await SpecialPowers.spawn(
                nestedIframe,
                [],
                async () => {
                  let secondIframeDivDims = content.document
                    .querySelector("div")
                    .getBoundingClientRect();

                  // Element within the nested iframe
                  secondIframeDivDims = {
                    left:
                      secondIframeDivDims.left +
                      content.document.defaultView.mozInnerScreenX,
                    top:
                      secondIframeDivDims.top +
                      content.document.defaultView.mozInnerScreenY,
                    width: secondIframeDivDims.width,
                    height: secondIframeDivDims.height,
                  };

                  return secondIframeDivDims;
                }
              );

              return [iframeDivDims, nestedIframeDivDims];
            }
          );

          // Offset each element position for the browser window
          for (let dims of iframesDivsDimArr) {
            dims.left -= content.window.mozInnerScreenX;
            dims.top -= content.window.mozInnerScreenY;
          }

          return [divDims].concat(iframesDivsDimArr);
        }
      );

      info(JSON.stringify(elementDimensions, null, 2));

      for (let el of elementDimensions) {
        let x = el.left + el.width / 2;
        let y = el.top + el.height / 2;

        mouse.move(x, y);
        await helper.waitForHoverElementRect(el.width, el.height);
        mouse.click(x, y);

        await helper.waitForStateChange(["selected"]);

        let dimensions = await helper.getSelectionRegionDimensions();

        is(
          dimensions.left,
          el.left,
          "The region left position matches the elements left position"
        );
        is(
          dimensions.top,
          el.top,
          "The region top position matches the elements top position"
        );
        is(
          dimensions.width,
          el.width,
          "The region width matches the elements width"
        );
        is(
          dimensions.height,
          el.height,
          "The region height matches the elements height"
        );

        mouse.click(500, 500);
        await helper.waitForStateChange(["crosshairs"]);
      }
    }
  );
});
