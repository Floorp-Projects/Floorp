/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_beforeinput_and_microtask() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,
      <html>
        <head>
          <script>
            function handleBeforeInput(event) {
              queueMicrotask(() => {
                event.preventDefault();
              });
            }
          </script>
        </head>
        <body onload="document.getElementsByTagName('input')[0].focus()">
        <input onbeforeinput="handleBeforeInput(event)"></body>
      </html>`,
    },
    async function (browser) {
      browser.focus();
      EventUtils.sendChar("a");
      await ContentTask.spawn(browser, [], function () {
        let val = content.document.body.firstElementChild.value;
        Assert.ok(
          val == "",
          "Element should have empty string as value [" + val + "]"
        );
      });
    }
  );
});
