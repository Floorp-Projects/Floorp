/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab("data:text/html;charset=utf8,<title>Test for Bug 1006027");

    const target = TargetFactory.forTab(tab);
    const hud = yield openConsole(tab);

    hud.jsterm.execute("console.log('bug1006027')");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.log",
        text: "bug1006027",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    });

    info('hud.outputNode.textContent:\n'+hud.outputNode.textContent);
    let timestampNodes = hud.outputNode.querySelectorAll('span.timestamp');
    let aTimestampMilliseconds = Array.prototype.map.call(timestampNodes,
      function (value) {
         // We are parsing timestamps as local time, relative to the begin of the epoch.
         // This is not the correct value of the timestamp, but good enough for comparison.
         return Date.parse('T'+String.trim(value.textContent));
      });

    let minTimestamp = Math.min.apply(null, aTimestampMilliseconds);
    let maxTimestamp = Math.max.apply(null, aTimestampMilliseconds);
    ok(Math.abs(maxTimestamp - minTimestamp) < 1000, "console.log message timestamp spread < 1000ms confirmed");
  }
}

