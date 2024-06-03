/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["layout.overflow-underflow.content.enabled", false],
      ["layout.overflow-underflow.content.enabled_in_addons", true],
    ],
  });
});

add_task(async function test_overflow_underflow_events_in_sidebar() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `<!DOCTYPE html>
         <html>
           <body>
             <div id="div" style="height: 10px; overflow-x: hidden">
               <div style="height: 10px"></div>
             </div>
             <script src="sidebar.js"></script>
           </body>
         </html>`,
      "sidebar.js": async function () {
        /* global div */
        let overflow = new Promise(resolve =>
          div.addEventListener("overflow", resolve, { once: true })
        );
        let underflow = new Promise(resolve =>
          div.addEventListener("underflow", resolve, { once: true })
        );
        div.style.height = "5px";
        await overflow;
        div.style.height = "10px";
        await underflow;
        browser.test.notifyPass("overflow/underflow events in sidebar");
      },
    },
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});
