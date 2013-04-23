/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/test/test-bug-837351-security-errors.html";

function run_test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, function testSecurityErrorLogged (hud) {
      let button = hud.ui.rootElement.querySelector(".webconsole-filter-button[category=\"security\"]");
      ok(button, "Found security button in the web console");

      waitForMessages({
        webconsole: hud,
        messages: [
          {
            name: "Logged blocking mixed active content",
            text: "Blocked loading mixed active content \"http://example.com/\"",
            category: CATEGORY_SECURITY,
            severity: SEVERITY_ERROR
          },
        ],
      }).then(finishTest);
    });
  }, true);
}

function test()
{
  SpecialPowers.pushPrefEnv({'set': [["security.mixed_content.block_active_content", true]]}, run_test);
}

