/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that async stacktraces are displayed as expected.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<script>
function timeout(cb, delay) {
  setTimeout(cb, delay);
}

function promiseThen(cb) {
  Promise.resolve().then(cb);
}

const onTimeout = () => {
  console.trace("Trace message");
  console.error("console error message");
  throw new Error("Thrown error message");
};
const onPromiseThen = () => timeout(onTimeout, 1);
promiseThen(onPromiseThen);

</script>`;

add_task(async function() {
  await pushPref("javascript.options.asyncstack", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  // Cached messages stacktrace are missing "promise callback" frames, so we reload
  // the page to get "live" messages instead. See Bug 1604428.
  await reloadPage();

  const expectedFrames = [
    "onTimeout",
    "(Async: setTimeout handler)",
    "timeout",
    "onPromiseThen",
    "(Async: promise callback)",
    "promiseThen",
    "<anonymous>",
  ].join("\n");

  const traceMsgNode = await waitFor(
    () => findMessage(hud, "Trace message", ".console-api.trace"),
    "Wait for the trace message to be logged"
  );
  let frames = await getSimplifiedStack(traceMsgNode);
  is(frames, expectedFrames, "console.trace has expected frames");

  const consoleErrorMsgNode = await waitFor(
    () => findMessage(hud, "console error message", ".console-api.error"),
    "Wait for the console error message to be logged"
  );
  consoleErrorMsgNode.querySelector(".arrow").click();
  frames = await getSimplifiedStack(consoleErrorMsgNode);
  is(frames, expectedFrames, "console.error has expected frames");

  const errorMsgNode = await waitFor(
    () =>
      findMessage(
        hud,
        "Uncaught Error: Thrown error message",
        ".javascript.error"
      ),
    "Wait for the thrown error message to be logged"
  );
  errorMsgNode.querySelector(".arrow").click();
  frames = await getSimplifiedStack(errorMsgNode);
  is(frames, expectedFrames, "thrown error has expected frames");
});

async function getSimplifiedStack(messageEl) {
  const framesEl = await waitFor(() => {
    const frames = messageEl.querySelectorAll(
      ".message-body-wrapper > .stacktrace .frame"
    );
    return frames.length > 0 ? frames : null;
  }, "Couldn't find stacktrace");

  return Array.from(framesEl)
    .map(frameEl =>
      Array.from(
        frameEl.querySelectorAll(".title,.location-async-cause")
      ).map(el => el.textContent.trim())
    )
    .flat()
    .join("\n");
}
