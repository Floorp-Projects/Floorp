/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This if the debugger's layout is correctly modified when the toolbox's
 * host changes.
 */

"use strict";

requestLongerTimeout(2);

let gDefaultHostType = Services.prefs.getCharPref("devtools.toolbox.host");

add_setup(async function() {
  // Disable window occlusion. See bug 1733955 / bug 1779559.
  if (navigator.platform.indexOf("Win") == 0) {
    await SpecialPowers.pushPrefEnv({
      set: [["widget.windows.window_occlusion_tracking.enabled", false]],
    });
  }
});

add_task(async function() {
  // test is too slow on some platforms due to the number of test cases
  const dbg = await initDebugger("doc-iframes.html");

  const layouts = [
    ["vertical", "window:small"],
    ["horizontal", "bottom"],
    ["vertical", "right"],
    ["horizontal", "window:big"],
  ];

  for (const layout of layouts) {
    const [orientation, host] = layout;
    await testLayout(dbg, orientation, host);
  }

  ok(true, "Orientations are correct");
});

async function testLayout(dbg, orientation, host) {
  info(`Switching to ${host} ${orientation}.`);

  await switchHost(dbg, host);
  await resizeToolboxWindow(dbg, host);
  return waitForState(
    dbg,
    state => dbg.selectors.getOrientation() == orientation
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
  const { toolbox } = dbg;
  const sizeOption = host.split(":")[1];
  if (!sizeOption) {
    return;
  }

  const win = toolbox.win.parent;

  let breakpoint = 800;
  if (sizeOption == "big" && win.outerWidth <= breakpoint) {
    breakpoint += 300;
  } else if (sizeOption == "small" && win.outerWidth >= breakpoint) {
    breakpoint -= 300;
  }
  resizeWindow(dbg, breakpoint);
}

function resizeWindow(dbg, width) {
  const { toolbox } = dbg;
  const win = toolbox.win.parent;
  win.resizeTo(width, window.screen.availHeight);
}

registerCleanupFunction(function() {
  Services.prefs.setCharPref("devtools.toolbox.host", gDefaultHostType);
  gDefaultHostType = null;
});
