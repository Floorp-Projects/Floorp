/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

const srcURL = new URL(`http://example.net/document-builder.sjs`);
srcURL.searchParams.append(
  "html",
  `<html>
    <head>
      <meta charset="utf-8"/>
      <title>OOP Document</title>
    </head>
    <body></body>
  </html>`
);
const iframeHTML = `<iframe title="OOP IFrame" src="${srcURL.href}"/>`;
const url = `data:text/html;charset=UTF-8,${encodeURIComponent(iframeHTML)}`;

const SVG_NS = "http://www.w3.org/2000/svg";
const STYLES_SHEET = `data:text/css;charset=utf-8,
.test-svg {
  position: absolute;
  width: 100%;
  height: 100%;
  pointer-events: none;
}

.test-path {
  opacity: 0.6;
}`;

add_task(async () => {
  await BrowserTestUtils.withNewTab(url, async browser => {
    const chromeWindow = gBrowser.ownerGlobal;
    const { documentElement } = chromeWindow.document;
    const iframe = chromeWindow.document.createElement("iframe");
    iframe.classList.add("devtools-highlighter-renderer");
    documentElement.append(iframe);
    if (
      iframe.contentWindow.readyState != "interactive" &&
      iframe.contentWindow.readyState != "complete"
    ) {
      await new Promise(resolve => {
        iframe.contentWindow.addEventListener("DOMContentLoaded", resolve, {
          once: true,
        });
      });
    }

    iframe.contentWindow.windowUtils.loadSheetUsingURIString(
      STYLES_SHEET,
      iframe.contentWindow.windowUtils.AGENT_SHEET
    );

    const svg = iframe.contentWindow.document.createElementNS(SVG_NS, "svg");
    svg.setAttribute("class", "test-svg");
    const path = iframe.contentWindow.document.createElementNS(SVG_NS, "path");
    path.setAttribute("class", "test-path");
    path.setAttributeNS(null, "fill", "#6a5acd");
    path.setAttributeNS(
      null,
      "d",
      `M${0},${0} L${innerWidth},${0} L${innerWidth},${innerHeight} L${0},${innerHeight}`
    );
    svg.appendChild(path);
    const anonContent = iframe.contentWindow.document.insertAnonymousContent(
      svg
    );

    await SpecialPowers.spawn(browser, [], () =>
      SpecialPowers.spawn(
        content.document.getElementsByTagName("iframe")[0],
        [],
        () => {
          docShell.chromeEventHandler.addEventListener(
            "click",
            () => {
              content.__clicked = true;
            },
            {
              capture: true,
              once: true,
            }
          );
        }
      )
    );

    const { width, height } = await SpecialPowers.spawn(browser, [], () =>
      content.document.getElementsByTagName("iframe")[0].getBoundingClientRect()
    );
    synthesizeNativeClick(browser, width / 2, height / 2);
    ok(
      await SpecialPowers.spawn(browser, [], async () => {
        const contentIframe = content.document.getElementsByTagName(
          "iframe"
        )[0];
        const clicked = await SpecialPowers.spawn(
          contentIframe,
          [],
          () => content.__clicked
        );

        // Clean up.
        contentIframe.remove();

        return clicked;
      }),
      "Mouse event was fired in the OOP iframe."
    );

    // Clean up
    iframe.contentWindow.document.removeAnonymousContent(anonContent);
    iframe.contentWindow.windowUtils.removeSheetUsingURIString(
      STYLES_SHEET,
      iframe.contentWindow.windowUtils.AGENT_SHEET
    );
    iframe.remove();
  });
});
