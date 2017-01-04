function* runTestOnPrivacyPrefPane(testFunc) {
  info("runTestOnPrivacyPrefPane entered");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences", true, true);
  let browser = tab.linkedBrowser;
  info("loaded about:preferences");
  browser.contentWindow.gotoPref("panePrivacy");
  info("viewing privacy pane, executing testFunc");
  testFunc(browser.contentWindow);
  yield BrowserTestUtils.removeTab(tab);
}

function controlChanged(element) {
  element.doCommand();
}

// We can only test the panes that don't trigger a preference update
function test_pane_visibility(win) {
  let modes = {
    "remember": "historyRememberPane",
    "custom": "historyCustomPane"
  };

  let historymode = win.document.getElementById("historyMode");
  ok(historymode, "history mode menulist should exist");
  let historypane = win.document.getElementById("historyPane");
  ok(historypane, "history mode pane should exist");

  for (let mode in modes) {
    historymode.value = mode;
    controlChanged(historymode);
    is(historypane.selectedPanel, win.document.getElementById(modes[mode]),
      "The correct pane should be selected for the " + mode + " mode");
    is_element_visible(historypane.selectedPanel,
                       "Correct pane should be visible for the " + mode + " mode");
  }
}

function test_dependent_elements(win) {
  let historymode = win.document.getElementById("historyMode");
  ok(historymode, "history mode menulist should exist");
  let pbautostart = win.document.getElementById("privateBrowsingAutoStart");
  ok(pbautostart, "the private browsing auto-start checkbox should exist");
  let controls = [
    win.document.getElementById("rememberHistory"),
    win.document.getElementById("rememberForms"),
    win.document.getElementById("keepUntil"),
    win.document.getElementById("keepCookiesUntil"),
    win.document.getElementById("alwaysClear"),
  ];
  controls.forEach(function(control) {
    ok(control, "the dependent controls should exist");
  });
  let independents = [
    win.document.getElementById("acceptCookies"),
    win.document.getElementById("acceptThirdPartyLabel"),
    win.document.getElementById("acceptThirdPartyMenu")
  ];
  independents.forEach(function(control) {
    ok(control, "the independent controls should exist");
  });
  let cookieexceptions = win.document.getElementById("cookieExceptions");
  ok(cookieexceptions, "the cookie exceptions button should exist");
  let keepuntil = win.document.getElementById("keepCookiesUntil");
  ok(keepuntil, "the keep cookies until menulist should exist");
  let alwaysclear = win.document.getElementById("alwaysClear");
  ok(alwaysclear, "the clear data on close checkbox should exist");
  let rememberhistory = win.document.getElementById("rememberHistory");
  ok(rememberhistory, "the remember history checkbox should exist");
  let rememberforms = win.document.getElementById("rememberForms");
  ok(rememberforms, "the remember forms checkbox should exist");
  let alwaysclearsettings = win.document.getElementById("clearDataSettings");
  ok(alwaysclearsettings, "the clear data settings button should exist");

  function expect_disabled(disabled) {
    controls.forEach(function(control) {
      is(control.disabled, disabled,
        control.getAttribute("id") + " should " + (disabled ? "" : "not ") + "be disabled");
    });
    is(keepuntil.value, disabled ? 2 : 0,
      "the keep cookies until menulist value should be as expected");
    if (disabled) {
     ok(!alwaysclear.checked,
        "the clear data on close checkbox value should be as expected");
     ok(!rememberhistory.checked,
        "the remember history checkbox value should be as expected");
     ok(!rememberforms.checked,
        "the remember forms checkbox value should be as expected");
    }
  }
  function check_independents(expected) {
    independents.forEach(function(control) {
      is(control.disabled, expected,
        control.getAttribute("id") + " should " + (expected ? "" : "not ") + "be disabled");
    });

    ok(!cookieexceptions.disabled,
      "the cookie exceptions button should never be disabled");
    ok(alwaysclearsettings.disabled,
      "the clear data settings button should always be disabled");
  }

  // controls should only change in custom mode
  historymode.value = "remember";
  controlChanged(historymode);
  expect_disabled(false);
  check_independents(false);

  // setting the mode to custom shouldn't change anything
  historymode.value = "custom";
  controlChanged(historymode);
  expect_disabled(false);
  check_independents(false);
}

function test_dependent_cookie_elements(win) {
  let historymode = win.document.getElementById("historyMode");
  ok(historymode, "history mode menulist should exist");
  let pbautostart = win.document.getElementById("privateBrowsingAutoStart");
  ok(pbautostart, "the private browsing auto-start checkbox should exist");
  let controls = [
    win.document.getElementById("acceptThirdPartyLabel"),
    win.document.getElementById("acceptThirdPartyMenu"),
    win.document.getElementById("keepUntil"),
    win.document.getElementById("keepCookiesUntil"),
  ];
  controls.forEach(function(control) {
    ok(control, "the dependent cookie controls should exist");
  });
  let acceptcookies = win.document.getElementById("acceptCookies");
  ok(acceptcookies, "the accept cookies checkbox should exist");

  function expect_disabled(disabled) {
    controls.forEach(function(control) {
      is(control.disabled, disabled,
        control.getAttribute("id") + " should " + (disabled ? "" : "not ") + "be disabled");
    });
  }

  historymode.value = "custom";
  controlChanged(historymode);
  pbautostart.checked = false;
  controlChanged(pbautostart);
  expect_disabled(false);

  acceptcookies.checked = false;
  controlChanged(acceptcookies);
  expect_disabled(true);

  acceptcookies.checked = true;
  controlChanged(acceptcookies);
  expect_disabled(false);

  let accessthirdparty = controls.shift();
  acceptcookies.checked = false;
  controlChanged(acceptcookies);
  expect_disabled(true);
  ok(accessthirdparty.disabled, "access third party button should be disabled");

  pbautostart.checked = false;
  controlChanged(pbautostart);
  expect_disabled(true);
  ok(accessthirdparty.disabled, "access third party button should be disabled");

  acceptcookies.checked = true;
  controlChanged(acceptcookies);
  expect_disabled(false);
  ok(!accessthirdparty.disabled, "access third party button should be enabled");
}

function test_dependent_clearonclose_elements(win) {
  let historymode = win.document.getElementById("historyMode");
  ok(historymode, "history mode menulist should exist");
  let pbautostart = win.document.getElementById("privateBrowsingAutoStart");
  ok(pbautostart, "the private browsing auto-start checkbox should exist");
  let alwaysclear = win.document.getElementById("alwaysClear");
  ok(alwaysclear, "the clear data on close checkbox should exist");
  let alwaysclearsettings = win.document.getElementById("clearDataSettings");
  ok(alwaysclearsettings, "the clear data settings button should exist");

  function expect_disabled(disabled) {
    is(alwaysclearsettings.disabled, disabled,
      "the clear data settings should " + (disabled ? "" : "not ") + "be disabled");
  }

  historymode.value = "custom";
  controlChanged(historymode);
  pbautostart.checked = false;
  controlChanged(pbautostart);
  alwaysclear.checked = false;
  controlChanged(alwaysclear);
  expect_disabled(true);

  alwaysclear.checked = true;
  controlChanged(alwaysclear);
  expect_disabled(false);

  alwaysclear.checked = false;
  controlChanged(alwaysclear);
  expect_disabled(true);
}

function test_dependent_prefs(win) {
  let historymode = win.document.getElementById("historyMode");
  ok(historymode, "history mode menulist should exist");
  let controls = [
    win.document.getElementById("rememberHistory"),
    win.document.getElementById("rememberForms"),
    win.document.getElementById("acceptCookies")
  ];
  controls.forEach(function(control) {
    ok(control, "the micro-management controls should exist");
  });

  let thirdPartyCookieMenu = win.document.getElementById("acceptThirdPartyMenu");
  ok(thirdPartyCookieMenu, "the third-party cookie control should exist");

  function expect_checked(checked) {
    controls.forEach(function(control) {
      is(control.checked, checked,
        control.getAttribute("id") + " should " + (checked ? "not " : "") + "be checked");
    });

    is(thirdPartyCookieMenu.value == "always" || thirdPartyCookieMenu.value == "visited", checked, "third-party cookies should " + (checked ? "not " : "") + "be limited");
  }

  // controls should be checked in remember mode
  historymode.value = "remember";
  controlChanged(historymode);
  expect_checked(true);

  // even if they're unchecked in custom mode
  historymode.value = "custom";
  controlChanged(historymode);
  thirdPartyCookieMenu.value = "never";
  controlChanged(thirdPartyCookieMenu);
  controls.forEach(function(control) {
    control.checked = false;
    controlChanged(control);
  });
  expect_checked(false);
  historymode.value = "remember";
  controlChanged(historymode);
  expect_checked(true);
}

function test_historymode_retention(mode, expect) {
  return function test_historymode_retention_fn(win) {
    let historymode = win.document.getElementById("historyMode");
    ok(historymode, "history mode menulist should exist");

    if ((historymode.value == "remember" && mode == "dontremember") ||
        (historymode.value == "dontremember" && mode == "remember") ||
        (historymode.value == "custom" && mode == "dontremember")) {
      return;
    }

    if (expect !== undefined) {
      is(historymode.value, expect,
        "history mode is expected to remain " + expect);
    }

    historymode.value = mode;
    controlChanged(historymode);
  };
}

function test_custom_retention(controlToChange, expect, valueIncrement) {
  return function test_custom_retention_fn(win) {
    let historymode = win.document.getElementById("historyMode");
    ok(historymode, "history mode menulist should exist");

    if (expect !== undefined) {
      is(historymode.value, expect,
        "history mode is expected to remain " + expect);
    }

    historymode.value = "custom";
    controlChanged(historymode);

    controlToChange = win.document.getElementById(controlToChange);
    ok(controlToChange, "the control to change should exist");
    switch (controlToChange.localName) {
    case "checkbox":
      controlToChange.checked = !controlToChange.checked;
      break;
    case "textbox":
      controlToChange.value = parseInt(controlToChange.value) + valueIncrement;
      break;
    case "menulist":
      controlToChange.value = valueIncrement;
      break;
    }
    controlChanged(controlToChange);
  };
}

function test_locbar_suggestion_retention(suggestion, autocomplete) {
  return function(win) {
    let elem = win.document.getElementById(suggestion + "Suggestion");
    ok(elem, "Suggest " + suggestion + " checkbox should exist.");
    elem.click();

    is(Services.prefs.getBoolPref("browser.urlbar.autocomplete.enabled"), autocomplete,
       "browser.urlbar.autocomplete.enabled pref should be " + autocomplete);
  };
}

const gPrefCache = new Map();

function cache_preferences(win) {
  let prefs = win.document.querySelectorAll("#privacyPreferences > preference");
  for (let pref of prefs)
    gPrefCache.set(pref.name, pref.value);
}

function reset_preferences(win) {
  let prefs = win.document.querySelectorAll("#privacyPreferences > preference");
  for (let pref of prefs)
    pref.value = gPrefCache.get(pref.name);
}

function run_test_subset(subset) {
  info("subset: " + Array.from(subset, x => x.name).join(",") + "\n");
  SpecialPowers.pushPrefEnv({"set": [["browser.preferences.instantApply", true]]});

  let tests = [cache_preferences, ...subset, reset_preferences];
  for (let test of tests) {
    add_task(runTestOnPrivacyPrefPane.bind(undefined, test));
  }
}
