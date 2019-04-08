/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This if the debugger's layout is correctly modified when the toolbox's
 * host changes.
 */
requestLongerTimeout(2);

var gDefaultHostType = Services.prefs.getCharPref("devtools.toolbox.host");

add_task(async function() {
  // test is too slow on some platforms due to the number of test cases
  const dbg = await initDebugger("doc-iframes.html");

  const layouts = [
    ["vertical", "window:small"],
    ["horizontal", "bottom"],
    ["vertical", "right"],
    ["horizontal", "window:big"]
  ];

  for (const layout of layouts) {
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
  const sizeOption = host.split(":")[1];
  const win = toolbox.win.parent;

  const breakpoint = 800;
  if (sizeOption == "big" && win.outerWidth <= breakpoint) {
    return resizeWindow(dbg, breakpoint + 300);
  } else if (sizeOption == "small" && win.outerWidth >= breakpoint) {
    return resizeWindow(dbg, breakpoint - 300);
  }
}

function resizeWindow(dbg, width) {
  const { panel, toolbox } = dbg;
  const win = toolbox.win.parent;
  const currentWidth = win.screen.width;
  win.resizeTo(width, window.screen.availHeight);
}

registerCleanupFunction(function() {
  Services.prefs.setCharPref("devtools.toolbox.host", gDefaultHostType);
  gDefaultHostType = null;
});
