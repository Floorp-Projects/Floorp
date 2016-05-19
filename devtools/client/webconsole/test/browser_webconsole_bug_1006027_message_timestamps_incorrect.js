/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab("data:text/html;charset=utf8,<title>Test for " +
                                "Bug 1006027");

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

    info("hud.outputNode.textContent:\n" + hud.outputNode.textContent);
    let timestampNodes = hud.outputNode.querySelectorAll("span.timestamp");
    let aTimestampMilliseconds = Array.prototype.map.call(timestampNodes,
      function (value) {
        // We are parsing timestamps as local time, relative to the begin of
        // the epoch.
        // This is not the correct value of the timestamp, but good enough for
        // comparison.
        return Date.parse("T" + String.trim(value.textContent));
      });

    let minTimestamp = Math.min.apply(null, aTimestampMilliseconds);
    let maxTimestamp = Math.max.apply(null, aTimestampMilliseconds);
    ok(Math.abs(maxTimestamp - minTimestamp) < 2000,
       "console.log message timestamp spread < 2000ms confirmed");
  }
}
