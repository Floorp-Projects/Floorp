/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar works properly

const TEST_URI = "data:text/html;charset=utf-8,<p>Tooltip Tests</p>";

function test() {
  addTab(TEST_URI, function() {
    Task.spawn(runTest).catch(err => {
      ok(false, ex);
      console.error(ex);
    }).then(finish);
  });
}

function* runTest() {
  info("Starting browser_toolbar_tooltip.js");

  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in runTest");

  let showPromise = observeOnce(DeveloperToolbar.NOTIFICATIONS.SHOW);
  document.getElementById("Tools:DevToolbar").doCommand();
  yield showPromise;

  let tooltipPanel = DeveloperToolbar.tooltipPanel;

  DeveloperToolbar.display.focusManager.helpRequest();
  yield DeveloperToolbar.display.inputter.setInput('help help');

  DeveloperToolbar.display.inputter.setCursor({ start: 'help help'.length });
  is(tooltipPanel._dimensions.start, 'help '.length,
          'search param start, when cursor at end');
  ok(getLeftMargin() > 30, 'tooltip offset, when cursor at end')

  DeveloperToolbar.display.inputter.setCursor({ start: 'help'.length });
  is(tooltipPanel._dimensions.start, 0,
          'search param start, when cursor at end of command');
  ok(getLeftMargin() > 9, 'tooltip offset, when cursor at end of command')

  DeveloperToolbar.display.inputter.setCursor({ start: 'help help'.length - 1 });
  is(tooltipPanel._dimensions.start, 'help '.length,
          'search param start, when cursor at penultimate position');
  ok(getLeftMargin() > 30, 'tooltip offset, when cursor at penultimate position')

  DeveloperToolbar.display.inputter.setCursor({ start: 0 });
  is(tooltipPanel._dimensions.start, 0,
          'search param start, when cursor at start');
  ok(getLeftMargin() > 9, 'tooltip offset, when cursor at start')
}

function getLeftMargin() {
  let style = DeveloperToolbar.tooltipPanel._panel.style.marginLeft;
  return parseInt(style.slice(0, -2), 10);
}

function observeOnce(topic, ownsWeak=false) {
  return new Promise(function(resolve, reject) {
    let resolver = function(subject) {
      Services.obs.removeObserver(resolver, topic);
      resolve(subject);
    };
    Services.obs.addObserver(resolver, topic, ownsWeak);
  }.bind(this));
}
