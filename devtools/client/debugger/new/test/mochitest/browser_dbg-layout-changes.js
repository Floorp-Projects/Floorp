/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This if the debugger's layout is correctly modified when the toolbox's
 * host changes.
 */

"use strict";

var gDefaultHostType = Services.prefs.getCharPref("devtools.toolbox.host");

add_task(async function() {
  // test is too slow on some platforms due to the number of test cases
  requestLongerTimeout(2);

  const dbg = await initDebugger("doc-iframes.html");

  const layouts = [
    ["horizontal", "bottom"],
    ["vertical", "side"],
    ["horizontal", "window:big"],
    ["vertical", "window:small"]
  ];

  for (let layout of layouts) {
    const [orientation, host] = layout;
    await testLayout(dbg, orientation, host);
  }

  ok(true, "Orientations are correct");
});

async function testLayout(dbg, orientation, host) {
  const { panel, toolbox } = dbg;
  info(`Switching to ${host} ${orientation}.`);

  await switchHost(dbg, host);
  await resizeToolboxWindow(dbg, host);
  return waitForState(
    dbg,
    state => dbg.selectors.getOrientation(state) == orientation
  );
}

function getHost(host) {
  if (host.indexOf("window") == 0) {
    return "window";
  }
  return host;
}

async function switchHost(dbg, hostType) {
  const { toolbox } = dbg;
  await toolbox.switchHost(getHost(hostType));
}

function resizeToolboxWindow(dbg, host) {
  const { panel, toolbox } = dbg;
  let sizeOption = host.split(":")[1];
  let win = toolbox.win.parent;

  let breakpoint = 700;
  if (sizeOption == "big" && win.outerWidth <= breakpoint) {
    return resizeWindow(dbg, breakpoint + 300);
  } else if (sizeOption == "small" && win.outerWidth >= breakpoint) {
    return resizeWindow(dbg, breakpoint - 300);
  }
}

function resizeWindow(dbg, width) {
  const { panel, toolbox } = dbg;
  let win = toolbox.win.parent;
  const currentWidth = win.screen.width;
  win.resizeTo(width, window.screen.availHeight);
}

registerCleanupFunction(function() {
  Services.prefs.setCharPref("devtools.toolbox.host", gDefaultHostType);
  gDefaultHostType = null;
});
