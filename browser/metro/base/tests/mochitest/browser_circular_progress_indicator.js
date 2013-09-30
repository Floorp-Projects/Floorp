/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;

function test() {
  waitForExplicitFinish();
  Task.spawn(function(){
    info(chromeRoot + "browser_progress_indicator.xul");
    yield addTab(chromeRoot + "browser_progress_indicator.xul");
    doc = Browser.selectedTab.browser.contentWindow.document;
  }).then(runTests);
}

gTests.push({
  desc: "circular progress indicator binding is applied.",
  run: function() {
    ok(doc, "doc got defined");

    let progressIndicator = doc.querySelector("#progress-indicator");
    ok(progressIndicator, "progress-indicator is found");
    is(typeof progressIndicator.reset, "function", "#progress-indicator has a reset() function");
    is(typeof progressIndicator.updateProgress, "function", "#progress-indicator has a updateProgress() function");
  }
});

gTests.push({
  desc: "start and end angles are correct for various percents complete",
  run: function() {
    let progressIndicator = doc.querySelector("#progress-indicator");
    ok(progressIndicator, "progress-indicator is found");
    is(typeof progressIndicator.updateProgress, "function", "#progress-indicator has a updateProgress() function");

    let expectedStartAngle = 1.5 * Math.PI;

    let percentComplete = 0;
    let [startAngle, endAngle] = progressIndicator.updateProgress(percentComplete);
    is(startAngle, expectedStartAngle, "start angle is correct");
    is(endAngle, startAngle + (2 * Math.PI * (percentComplete / 100)), "end angle is correct");

    percentComplete = 0.05;
    [startAngle, endAngle] = progressIndicator.updateProgress(percentComplete);
    is(startAngle, expectedStartAngle, "start angle is correct");
    is(endAngle, startAngle + (2 * Math.PI * (percentComplete / 100)), "end angle is correct");

    percentComplete = 0.5;
    [startAngle, endAngle] = progressIndicator.updateProgress(percentComplete);
    is(startAngle, expectedStartAngle, "start angle is correct");
    is(endAngle, startAngle + (2 * Math.PI * (percentComplete / 100)), "end angle is correct");

    percentComplete = 1;
    [startAngle, endAngle] = progressIndicator.updateProgress(percentComplete);
    is(startAngle, expectedStartAngle, "start angle is correct");
    is(endAngle, startAngle + (2 * Math.PI * (percentComplete / 100)), "end angle is correct");
  }
});