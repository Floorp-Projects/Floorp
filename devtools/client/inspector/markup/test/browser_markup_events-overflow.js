/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URL = URL_ROOT + "doc_markup_events-overflow.html";
const TEST_DATA = [
  {
    desc: "editor overflows container",
    // scroll to bottom
    initialScrollTop: -1,
    // last header
    headerToClick: 49,
    alignBottom: true,
    alignTop: false,
  },
  {
    desc: "header overflows the container",
    initialScrollTop: 2,
    headerToClick: 0,
    alignBottom: false,
    alignTop: true,
  },
  {
    desc: "neither header nor editor overflows the container",
    initialScrollTop: 2,
    headerToClick: 5,
    alignBottom: false,
    alignTop: false,
  },
];

add_task(async function() {
  let { inspector } = await openInspectorForURL(TEST_URL);

  let markupContainer = await getContainerForSelector("#events", inspector);
  let evHolder = markupContainer.elt.querySelector(".markupview-event-badge");
  let tooltip = inspector.markup.eventDetailsTooltip;

  info("Clicking to open event tooltip.");
  EventUtils.synthesizeMouseAtCenter(evHolder, {},
    inspector.markup.doc.defaultView);
  await tooltip.once("shown");
  info("EventTooltip visible.");

  let container = tooltip.panel;
  let containerRect = container.getBoundingClientRect();
  let headers = container.querySelectorAll(".event-header");

  for (let data of TEST_DATA) {
    info("Testing scrolling when " + data.desc);

    if (data.initialScrollTop < 0) {
      info("Scrolling container to the bottom.");
      let newScrollTop = container.scrollHeight - container.clientHeight;
      data.initialScrollTop = container.scrollTop = newScrollTop;
    } else {
      info("Scrolling container by " + data.initialScrollTop + "px");
      container.scrollTop = data.initialScrollTop;
    }

    is(container.scrollTop, data.initialScrollTop, "Container scrolled.");

    info("Clicking on header #" + data.headerToClick);
    let header = headers[data.headerToClick];

    let ready = tooltip.once("event-tooltip-ready");
    EventUtils.synthesizeMouseAtCenter(header, {}, header.ownerGlobal);
    await ready;

    info("Event handler expanded.");

    // Wait for any scrolling to finish.
    await promiseNextTick();

    if (data.alignTop) {
      let headerRect = header.getBoundingClientRect();

      is(Math.round(headerRect.top), Math.round(containerRect.top),
        "Clicked header is aligned with the container top.");
    } else if (data.alignBottom) {
      let editorRect = header.nextElementSibling.getBoundingClientRect();

      is(Math.round(editorRect.bottom), Math.round(containerRect.bottom),
        "Clicked event handler code is aligned with the container bottom.");
    } else {
      is(container.scrollTop, data.initialScrollTop,
        "Container did not scroll, as expected.");
    }
  }
});
