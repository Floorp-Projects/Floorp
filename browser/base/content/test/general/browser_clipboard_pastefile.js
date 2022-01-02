// This test is used to check that pasting files removes all non-file data from
// event.clipboardData.

add_task(async function() {
  var input = document.createElement("input");
  document.documentElement.appendChild(input);

  input.focus();
  input.value = "Text";
  input.select();

  await new Promise((resolve, reject) => {
    input.addEventListener(
      "copy",
      function(event) {
        event.clipboardData.setData("text/plain", "Alternate");
        // For this test, it doesn't matter that the file isn't actually a file.
        event.clipboardData.setData("application/x-moz-file", "Sample");
        event.preventDefault();
        resolve();
      },
      { capture: true, once: true }
    );

    EventUtils.synthesizeKey("c", { accelKey: true });
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/base/content/test/general/clipboard_pastefile.html"
  );
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function(arg) {
    content.document.getElementById("input").focus();
  });

  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);

  let output = await SpecialPowers.spawn(browser, [], async function(arg) {
    return content.document.getElementById("output").textContent;
  });
  is(output, "Passed", "Paste file");

  input.focus();

  await new Promise((resolve, reject) => {
    input.addEventListener(
      "paste",
      function(event) {
        let dt = event.clipboardData;
        is(dt.types.length, 3, "number of types");
        ok(dt.types.includes("text/plain"), "text/plain exists in types");
        ok(
          dt.mozTypesAt(0).contains("text/plain"),
          "text/plain exists in mozTypesAt"
        );
        is(
          dt.getData("text/plain"),
          "Alternate",
          "text/plain returned in getData"
        );
        is(
          dt.mozGetDataAt("text/plain", 0),
          "Alternate",
          "text/plain returned in mozGetDataAt"
        );

        resolve();
      },
      { capture: true, once: true }
    );

    EventUtils.synthesizeKey("v", { accelKey: true });
  });

  input.remove();

  BrowserTestUtils.removeTab(tab);
});
