"use strict";

let tests = [ "about:robots?foo", "about:robots#foo", "about:robots?foo#bar"];
tests.forEach(async test => {
  add_task(async () => {
    await BrowserTestUtils.withNewTab(test, async browser => {
      await ContentTask.spawn(browser, null, () => {
        is(content.document.nodePrincipal.origin, "about:robots");
      });
    });
  });
});
