async function waitForPdfJS(browser, url) {
  await SpecialPowers.pushPrefEnv({
    set: [["pdfjs.eventBusDispatchToDOM", true]],
  });
  // Runs tests after all "load" event handlers have fired off
  return ContentTask.spawn(browser, url, async function(contentUrl) {
    await new Promise(resolve => {
      // NB: Add the listener to the global object so that we receive the
      // event fired from the new window.
      addEventListener(
        "documentloaded",
        function listener() {
          removeEventListener("documentloaded", listener, false);
          resolve();
        },
        false,
        true
      );

      content.location = contentUrl;
    });
  });
}
