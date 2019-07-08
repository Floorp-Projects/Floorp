/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8><script>
    var bar = () => myFunc();
    var rab = () => myFunc();
    var myFunc = () => console.trace();

    bar();bar();
    rab();rab();
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findMessage(hud, "trace"));
  ok(true, "console.trace() message is displayed in the console");
  const messages = findMessages(hud, "console.trace()");
  is(messages.length, 4, "There are 4 console.trace() messages");

  info("Wait until the stacktraces are displayed");
  await waitFor(() => getFrames(hud).length === messages.length);
  const [traceBar1, traceBar2, traceRab1, traceRab2] = getFrames(hud);

  const framesBar1 = getFramesTitleFromTrace(traceBar1);
  is(
    framesBar1.join(" - "),
    "myFunc - bar - <anonymous>",
    "First bar trace has the expected frames"
  );

  const framesBar2 = getFramesTitleFromTrace(traceBar2);
  is(
    framesBar2.join(" - "),
    "myFunc - bar - <anonymous>",
    "Second bar trace has the expected frames"
  );

  const framesRab1 = getFramesTitleFromTrace(traceRab1);
  is(
    framesRab1.join(" - "),
    "myFunc - rab - <anonymous>",
    "First rab trace has the expected frames"
  );

  const framesRab2 = getFramesTitleFromTrace(traceRab2);
  is(
    framesRab2.join(" - "),
    "myFunc - rab - <anonymous>",
    "Second rab trace has the expected frames"
  );
});

/**
 * Get all the stacktrace `.frames` elements  displayed in the console output.
 * @returns {Array<HTMLElement>}
 */
function getFrames(hud) {
  return Array.from(hud.ui.outputNode.querySelectorAll(".stacktrace .frames"));
}

/**
 * Given a stacktrace element, return an array of the frame names displayed in it.
 * @param {HTMLElement} traceEl
 * @returns {Array<String>}
 */
function getFramesTitleFromTrace(traceEl) {
  return Array.from(traceEl.querySelectorAll(".frame .title")).map(
    t => t.textContent
  );
}
