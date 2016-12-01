/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(function* test() {
  const URL = "data:text/html,<iframe width='700' height='700'></iframe>";
  yield BrowserTestUtils.withNewTab({ gBrowser, url: URL }, function* (browser) {
    yield ContentTask.spawn(browser,
                            { is_element_hidden_: is_element_hidden.toSource(),
                              is_hidden_: is_hidden.toSource() },
    function* ({ is_element_hidden_, is_hidden_ }) {
      let loadError =
        ContentTaskUtils.waitForEvent(this, "AboutNetErrorLoad", false, null, true);
      let iframe = content.document.querySelector("iframe");
      iframe.src = "https://expired.example.com/";

      yield loadError;

      let is_hidden = eval(`(() => ${is_hidden_})()`);
      let is_element_hidden = eval(`(() => ${is_element_hidden_})()`);
      let doc = content.document.getElementsByTagName("iframe")[0].contentDocument;
      let aP = doc.getElementById("badCertAdvancedPanel");
      ok(aP, "Advanced content should exist");
      void is_hidden; // Quiet eslint warnings (actual use under is_element_hidden)
      is_element_hidden(aP, "Advanced content should not be visible by default")
    });
  });
});
