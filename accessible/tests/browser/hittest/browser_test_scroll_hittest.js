/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Verify that hit testing returns the proper accessible when one accessible
 * covers another accessible due to scroll clipping. See Bug 1819741.
 */
addAccessibleTask(
  `
<div id="container" style="height: 100%; position: absolute; flex-direction: column; display: flex;">
  <div id="title-bar" style="height: 500px; background-color: red;"></div>
  <div id="message-container" style="overflow-y: hidden; display: flex;">
    <div style="overflow-y: auto;" id="message-scrollable">
      <p style="white-space: pre-line;">
        Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec dictum luctus molestie. Nam in libero mi. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.

        Praesent aliquet semper libero, eu ullamcorper tortor vestibulum ac. Sed non pharetra sem. Quisque sodales ipsum a ipsum condimentum porttitor. Integer luctus pellentesque ipsum, eu dignissim nunc fermentum in.

        Etiam blandit nisl vitae dolor molestie faucibus. In euismod, massa vitae commodo bibendum, urna augue pharetra nibh, et sagittis libero est in ligula. Mauris tincidunt risus ornare, rutrum augue in, blandit ligula. Aenean ultrices vel risus sit amet varius.

        Vivamus pretium ultricies nisi a cursus. Integer cursus quam a metus ultricies, vel pulvinar nunc varius. Quisque facilisis lorem eget ipsum vehicula, laoreet congue lorem viverra.

        Praesent dignissim, diam sed semper ultricies, diam ex laoreet justo, ac euismod massa metus pharetra nunc. Vestibulum sapien erat, consequat at eleifend id, suscipit sit amet mi.

        Curabitur sed mauris vitae justo rutrum convallis ac sed justo. Ut nec est sed nisi feugiat egestas. Mauris accumsan mi eget nibh fermentum, in dignissim odio feugiat.

        Maecenas augue dolor, gravida ut ultrices ultricies, condimentum et dui. In sed augue fermentum, posuere velit et, pulvinar tellus. Morbi id fermentum quam, at varius arcu.

        Duis elementum vitae sapien id tincidunt. Aliquam velit ligula, sollicitudin eget placerat non, aliquam at erat. Pellentesque non porta metus. Mauris vel finibus sem, nec ullamcorper leo.

        Nulla sit amet lorem vitae diam consectetur porttitor a cursus massa. Sed id ornare lorem. Sed placerat facilisis ipsum et ultricies. Sed eu semper enim, ut aliquet odio.

        Sed nulla ex, pharetra vel porttitor congue, dictum et purus. Suspendisse vel risus sit amet nulla volutpat ullamcorper. Morbi et ullamcorper est. Pellentesque eget porta risus. Nullam non felis elementum, auctor massa et, consectetur neque.

        Fusce sit amet arcu finibus, ornare sem sed, tempus nibh. Donec rutrum odio eget bibendum pulvinar. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus.

        Phasellus sed risus diam. Vivamus mollis, risus ac feugiat pellentesque, ligula tortor finibus libero, et venenatis turpis lectus et justo. Suspendisse euismod mi at lectus sagittis dignissim. Mauris a ornare enim.
      </p>
    </div>
  </div>
  <div id="footer-bar" style="height: 500px; background-color: blue;"></div>
</div>
  `,
  async function (browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container");
    const scrollable = findAccessibleChildByID(docAcc, "message-scrollable");
    const titleBar = findAccessibleChildByID(docAcc, "title-bar");
    const footerBar = findAccessibleChildByID(docAcc, "footer-bar");
    const dpr = await getContentDPR(browser);
    const [, , , titleBarHeight] = Layout.getBounds(titleBar, dpr);
    const [, , , scrollableHeight] = Layout.getBounds(scrollable, dpr);

    // Verify that the child at this point is not the underlying paragraph.
    info(
      "Testing that the deepest child at this point is the overlaid section, not the paragraph beneath it."
    );
    await testChildAtPoint(
      dpr,
      1,
      titleBarHeight - 1,
      container,
      titleBar,
      titleBar
    );
    await testChildAtPoint(
      dpr,
      1,
      titleBarHeight + scrollableHeight + 1,
      container,
      footerBar,
      footerBar
    );

    await invokeContentTask(browser, [], () => {
      // Scroll the text down.
      let elem = content.document.getElementById("message-scrollable");
      elem.scrollTo(0, elem.scrollHeight);
    });
    await waitForContentPaint(browser);

    info(
      "Testing that the deepest child at this point is still the overlaid section, after scrolling the paragraph."
    );
    await testChildAtPoint(
      dpr,
      1,
      titleBarHeight - 1,
      container,
      titleBar,
      titleBar
    );
    await testChildAtPoint(
      dpr,
      1,
      titleBarHeight + scrollableHeight + 1,
      container,
      footerBar,
      footerBar
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);
