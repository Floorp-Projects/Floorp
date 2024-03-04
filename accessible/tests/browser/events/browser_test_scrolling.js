/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
    <div style="height: 100vh" id="one">one</div>
    <div style="height: 100vh" id="two">two</div>
    <div style="height: 100vh; width: 200vw; overflow: auto;" id="three">
      <div style="height: 300%;">three</div>
    </div>
    <textarea id="textarea" rows="1">a
b
c</textarea>
  `,
  async function (browser, accDoc) {
    let onScrolling = waitForEvents([
      [EVENT_SCROLLING, accDoc],
      [EVENT_SCROLLING_END, accDoc],
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.location.hash = "#two";
    });
    let [scrollEvent1, scrollEndEvent1] = await onScrolling;
    scrollEvent1.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEvent1.maxScrollY,
      scrollEvent1.scrollY,
      "scrollY is within max"
    );
    scrollEndEvent1.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEndEvent1.maxScrollY,
      scrollEndEvent1.scrollY,
      "scrollY is within max"
    );

    onScrolling = waitForEvents([
      [EVENT_SCROLLING, accDoc],
      [EVENT_SCROLLING_END, accDoc],
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.location.hash = "#three";
    });
    let [scrollEvent2, scrollEndEvent2] = await onScrolling;
    scrollEvent2.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greater(
      scrollEvent2.scrollY,
      scrollEvent1.scrollY,
      `${scrollEvent2.scrollY} > ${scrollEvent1.scrollY}`
    );
    scrollEndEvent2.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEndEvent2.maxScrollY,
      scrollEndEvent2.scrollY,
      "scrollY is within max"
    );

    onScrolling = waitForEvents([
      [EVENT_SCROLLING, accDoc],
      [EVENT_SCROLLING_END, accDoc],
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.scrollTo(10, 0);
    });
    let [scrollEvent3, scrollEndEvent3] = await onScrolling;
    scrollEvent3.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEvent3.maxScrollX,
      scrollEvent3.scrollX,
      "scrollX is within max"
    );
    scrollEndEvent3.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEndEvent3.maxScrollX,
      scrollEndEvent3.scrollX,
      "scrollY is within max"
    );
    Assert.greater(
      scrollEvent3.scrollX,
      scrollEvent2.scrollX,
      `${scrollEvent3.scrollX} > ${scrollEvent2.scrollX}`
    );

    // non-doc scrolling
    onScrolling = waitForEvents([
      [EVENT_SCROLLING, "three"],
      [EVENT_SCROLLING_END, "three"],
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.document.querySelector("#three").scrollTo(0, 10);
    });
    let [scrollEvent4, scrollEndEvent4] = await onScrolling;
    scrollEvent4.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEvent4.maxScrollY,
      scrollEvent4.scrollY,
      "scrollY is within max"
    );
    scrollEndEvent4.QueryInterface(nsIAccessibleScrollingEvent);
    Assert.greaterOrEqual(
      scrollEndEvent4.maxScrollY,
      scrollEndEvent4.scrollY,
      "scrollY is within max"
    );

    // textarea scrolling
    info("Moving textarea caret to c");
    onScrolling = waitForEvents([
      [EVENT_SCROLLING, "textarea"],
      [EVENT_SCROLLING_END, "textarea"],
    ]);
    await invokeContentTask(browser, [], () => {
      const textareaDom = content.document.getElementById("textarea");
      textareaDom.focus();
      textareaDom.selectionStart = 4;
    });
    await onScrolling;
  }
);

// Verify that the scrolling start event is fired for an anchor change.
addAccessibleTask(
  `
    <p>a</p>
    <p>b</p>
    <p id="c">c</p>
  `,
  async function (browser) {
    let onScrollingStart = waitForEvent(EVENT_SCROLLING_START, "c");
    await SpecialPowers.spawn(browser, [], () => {
      content.location.hash = "#c";
    });
    await onScrollingStart;
  },
  { chrome: true, topLevel: true }
);

// Ensure that a scrollable, focused non-interactive element receives a
// scrolling start event when an anchor jump to that element is triggered.
addAccessibleTask(
  `
<div style="height: 100vh; width: 100vw; overflow: auto;" id="scrollable">
  <h1 style="height: 300%;" id="inside-scrollable">test</h1>
</div>
  `,
  async function (browser) {
    let onScrollingStart = waitForEvent(
      EVENT_SCROLLING_START,
      "inside-scrollable"
    );
    await invokeContentTask(browser, [], () => {
      const scrollable = content.document.getElementById("scrollable");
      scrollable.focus();
      content.location.hash = "#inside-scrollable";
    });
    await onScrollingStart;
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
