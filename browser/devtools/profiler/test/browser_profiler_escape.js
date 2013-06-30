/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, function (tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    let record = gPanel.controls.record;

    gPanel.once("started", () => {
      gPanel.once("stopped", () => {
        let [ win, doc ] = getProfileInternals(gPanel.activeProfile.uid);

        let expl = "<script>function f() {}</script></textarea><img/src='about:logo'>";
        let expl2 = "<script>function f() {}</script></pre><img/src='about:logo'>";

        is(win.escapeHTML(expl),
          "&lt;script&gt;function f() {}&lt;/script&gt;&lt;/textarea&gt;&lt;img/src='about:logo'&gt;");

        is(win.escapeHTML(expl2),
          "&lt;script&gt;function f() {}&lt;/script&gt;&lt;/pre&gt;&lt;img/src='about:logo'&gt;");

        tearDown(gTab, () => {
          gTab = null;
          gPanel = null;
        });
      });

      setTimeout(() => {
        record.click();
      }, 50);
    });

    record.click();
  });
}