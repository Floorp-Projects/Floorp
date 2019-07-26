/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the text filter box works to filter based on location.

"use strict";

// In this test, we are trying to test the filtering functionality of the filter
// input. We test filtering not only for the contents of logs themseleves but
// also for the filenames.
//
// We simulate an HTML file which executes two Javascript files, one with an
// ASCII filename outputs some ASCII logs and the other one with a Unicode
// filename outputs some Unicode logs.

const SEASON = {
  english: "season",
  chinese: "\u5b63",
};
const SEASONS = [
  {
    english: "spring",
    chinese: "\u6625",
    escapedChinese: "\\u6625",
  },
  {
    english: "summer",
    chinese: "\u590f",
    escapedChinese: "\\u590f",
  },
  {
    english: "autumn",
    chinese: "\u79cb",
    escapedChinese: "\\u79cb",
  },
  {
    english: "winter",
    chinese: "\u51ac",
    escapedChinese: "\\u51ac",
  },
];

// filenames
const HTML_FILENAME = `test.html`;
const JS_ASCII_FILENAME = `${SEASON.english}.js`;
const JS_UNICODE_FILENAME = `${SEASON.chinese}.js`;
const ENCODED_JS_UNICODE_FILENAME = encodeURIComponent(JS_UNICODE_FILENAME);

// file contents
const HTML_CONSOLE_OUTPUT = `Test filtering ${SEASON.english} names.`;
const HTML_CONTENT = `<!DOCTYPE html>
<meta charset="utf-8">
<title>Test filtering logs by filling keywords in the filter input.</title>
<script>
console.log("${HTML_CONSOLE_OUTPUT}");
</script>
<script src="/${JS_ASCII_FILENAME}"></script>
<script src="/${ENCODED_JS_UNICODE_FILENAME}"></script>`;

add_task(async function() {
  const testUrl = createServerAndGetTestUrl();
  const hud = await openNewTabAndConsole(testUrl);

  // Let's wait for the last logged message of each file to be displayed in the
  // output, in order to make sure all the logged messages have been displayed.
  const lastSeason = SEASONS[SEASONS.length - 1];
  await waitFor(
    () =>
      findMessage(hud, lastSeason.english) &&
      findMessage(hud, lastSeason.chinese)
  );

  // One external Javascript file outputs every season name in English, and the
  // other Javascript file outputs every season name in Chinese.
  // The HTML file outputs one line on its own.
  // So the total number of all the logs is the doubled number of seasons plus
  // one.
  let visibleLogs = getVisibleLogs(hud);
  is(
    visibleLogs.length,
    SEASONS.length * 2 + 1,
    "the total number of all the logs before starting filtering"
  );
  checkLogContent(visibleLogs[0], HTML_CONSOLE_OUTPUT, HTML_FILENAME);
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(visibleLogs[i + 1], SEASONS[i].english, JS_ASCII_FILENAME);
  }
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(
      visibleLogs[i + 1 + SEASONS.length],
      SEASONS[i].chinese,
      JS_UNICODE_FILENAME
    );
  }
  // checking the visibility of clear button, it should be visible only when
  // there is text inside filter input box
  await setFilterState(hud, { text: "" });
  is(getClearButton(hud).hidden, true, "Clear button is hidden");
  await setFilterState(hud, { text: JS_ASCII_FILENAME });
  is(getClearButton(hud).hidden, false, "Clear button is visible");

  // All the logs outputted by the ASCII Javascript file are visible, the others
  // are hidden.
  await setFilterState(hud, { text: JS_ASCII_FILENAME });
  visibleLogs = getVisibleLogs(hud);
  is(
    visibleLogs.length,
    SEASONS.length,
    `the number of all the logs containing ${JS_ASCII_FILENAME}`
  );
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(visibleLogs[i], SEASONS[i].english, JS_ASCII_FILENAME);
  }

  // Every season name in English is outputted once.
  for (const curSeason of SEASONS) {
    await setFilterState(hud, { text: curSeason.english });
    visibleLogs = getVisibleLogs(hud);
    is(
      visibleLogs.length,
      1,
      `the number of all the logs containing ${curSeason.english}`
    );
    checkLogContent(visibleLogs[0], curSeason.english, JS_ASCII_FILENAME);
  }

  // All the logs outputted by the Unicode Javascript file are visible, the
  // others are hidden.
  await setFilterState(hud, { text: JS_UNICODE_FILENAME });
  visibleLogs = getVisibleLogs(hud);
  is(
    visibleLogs.length,
    SEASONS.length,
    `the number of all the logs containing ${JS_UNICODE_FILENAME}`
  );
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(visibleLogs[i], SEASONS[i].chinese, JS_UNICODE_FILENAME);
  }

  // Every season name in Chinese is outputted once.
  for (const curSeason of SEASONS) {
    await setFilterState(hud, { text: curSeason.chinese });
    visibleLogs = getVisibleLogs(hud);
    is(
      visibleLogs.length,
      1,
      `the number of all the logs containing ${curSeason.chinese}`
    );
    checkLogContent(visibleLogs[0], curSeason.chinese, JS_UNICODE_FILENAME);
  }

  // The filename of the ASCII Javascript file contains the English word season,
  // so all the logs outputted by the file are visible, besides, the HTML
  // outputs one line containing the English word season, so it is also visible.
  // The other logs are hidden. So the number of all the visible logs is the
  // season number plus one.
  await setFilterState(hud, { text: SEASON.english });
  visibleLogs = getVisibleLogs(hud);
  is(
    visibleLogs.length,
    SEASONS.length + 1,
    `the number of all the logs containing ${SEASON.english}`
  );
  checkLogContent(visibleLogs[0], HTML_CONSOLE_OUTPUT, HTML_FILENAME);
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(visibleLogs[i + 1], SEASONS[i].english, JS_ASCII_FILENAME);
  }

  // The filename of the Unicode Javascript file contains the Chinese word
  // season, so all the logs outputted by the file are visible. The other logs
  // are hidden. So the number of all the visible logs is the season number.
  await setFilterState(hud, { text: SEASON.chinese });
  visibleLogs = getVisibleLogs(hud);
  is(
    visibleLogs.length,
    SEASONS.length,
    `the number of all the logs containing ${SEASON.chinese}`
  );
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(visibleLogs[i], SEASONS[i].chinese, JS_UNICODE_FILENAME);
  }

  // After clearing the text in the filter input box, all the logs are visible
  // again.
  await setFilterState(hud, { text: "" });
  checkAllMessagesAreVisible(hud);

  // clearing the text in the filter input box using clear button, so after which
  // all logs will be visible again
  await setFilterState(hud, { text: JS_ASCII_FILENAME });

  info("Click the input clear button");
  clickClearButton(hud);
  await waitFor(() => getClearButton(hud).hidden === true);
  checkAllMessagesAreVisible(hud);
});

// Create an HTTP server to simulate a response for the a URL request and return
// the URL.
function createServerAndGetTestUrl() {
  const httpServer = createTestHTTPServer();

  httpServer.registerContentType("html", "text/html");
  httpServer.registerContentType("js", "application/javascript");

  httpServer.registerPathHandler("/" + HTML_FILENAME, function(
    request,
    response
  ) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(HTML_CONTENT);
  });
  httpServer.registerPathHandler("/" + JS_ASCII_FILENAME, function(
    request,
    response
  ) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/javascript", false);
    let content = "";
    for (const curSeason of SEASONS) {
      content += `console.log("${curSeason.english}");`;
    }
    response.write(content);
  });
  httpServer.registerPathHandler("/" + ENCODED_JS_UNICODE_FILENAME, function(
    request,
    response
  ) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/javascript", false);
    let content = "";
    for (const curSeason of SEASONS) {
      content += `console.log("${curSeason.escapedChinese}");`;
    }
    response.write(content);
  });
  const port = httpServer.identity.primaryPort;
  return `http://localhost:${port}/${HTML_FILENAME}`;
}

function getClearButton(hud) {
  return hud.ui.outputNode.querySelector(
    ".devtools-searchbox .devtools-searchinput-clear"
  );
}

function clickClearButton(hud) {
  getClearButton(hud).click();
}

function getVisibleLogs(hud) {
  const outputNode = hud.ui.outputNode;
  return outputNode.querySelectorAll(".message");
}

function checkAllMessagesAreVisible(hud) {
  const visibleLogs = getVisibleLogs(hud);
  is(
    visibleLogs.length,
    SEASONS.length * 2 + 1,
    "the total number of all the logs after clearing filtering"
  );
  checkLogContent(visibleLogs[0], HTML_CONSOLE_OUTPUT, HTML_FILENAME);
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(visibleLogs[i + 1], SEASONS[i].english, JS_ASCII_FILENAME);
  }
  for (let i = 0; i < SEASONS.length; i++) {
    checkLogContent(
      visibleLogs[i + 1 + SEASONS.length],
      SEASONS[i].chinese,
      JS_UNICODE_FILENAME
    );
  }
}
/**
 * Check if the content of a log message is what we expect.
 *
 * @param Object node
 *        The node for the log message.
 * @param String expectedMessageBody
 *        The string we expect to match the message body in the log message.
 * @param String expectedFilename
 *        The string we expect to match the filename in the log message.
 */
function checkLogContent(node, expectedMessageBody, expectedFilename) {
  const messageBody = node.querySelector(".message-body").textContent;
  const location = node.querySelector(".message-location").textContent;
  // The location detail contains the line number and the column number, let's
  // strip them to get the filename.
  const filename = location.split(":")[0];

  is(messageBody, expectedMessageBody, "the expected message body");
  is(filename, expectedFilename, "the expected filename");
}
