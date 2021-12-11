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
    </div>`,
  async function(browser, accDoc) {
    let onScrolling = waitForEvents([
      [EVENT_SCROLLING, accDoc],
      [EVENT_SCROLLING_END, accDoc],
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.location.hash = "#two";
    });
    let [scrollEvent1, scrollEndEvent1] = await onScrolling;
    scrollEvent1.QueryInterface(nsIAccessibleScrollingEvent);
    ok(
      scrollEvent1.maxScrollY >= scrollEvent1.scrollY,
      "scrollY is within max"
    );
    scrollEndEvent1.QueryInterface(nsIAccessibleScrollingEvent);
    ok(
      scrollEndEvent1.maxScrollY >= scrollEndEvent1.scrollY,
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
    ok(
      scrollEvent2.scrollY > scrollEvent1.scrollY,
      `${scrollEvent2.scrollY} > ${scrollEvent1.scrollY}`
    );
    scrollEndEvent2.QueryInterface(nsIAccessibleScrollingEvent);
    ok(
      scrollEndEvent2.maxScrollY >= scrollEndEvent2.scrollY,
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
    ok(
      scrollEvent3.maxScrollX >= scrollEvent3.scrollX,
      "scrollX is within max"
    );
    scrollEndEvent3.QueryInterface(nsIAccessibleScrollingEvent);
    ok(
      scrollEndEvent3.maxScrollX >= scrollEndEvent3.scrollX,
      "scrollY is within max"
    );
    ok(
      scrollEvent3.scrollX > scrollEvent2.scrollX,
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
    ok(
      scrollEvent4.maxScrollY >= scrollEvent4.scrollY,
      "scrollY is within max"
    );
    scrollEndEvent4.QueryInterface(nsIAccessibleScrollingEvent);
    ok(
      scrollEndEvent4.maxScrollY >= scrollEndEvent4.scrollY,
      "scrollY is within max"
    );
  }
);
