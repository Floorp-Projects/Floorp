function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(allTabs.prefName, true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(allTabs.prefName);
  });

  allTabs.init();
  nextSequence();
}

var sequences = 3;
var chars = "ABCDEFGHI";
var closedTabs;
var history;
var steps;
var whenOpen = [
  startSearch,
  clearSearch, clearSearch,
  closeTab,
  moveTab,
  closePanel,
];
var whenClosed = [
  openPanel, openPanel, openPanel, openPanel, openPanel, openPanel,
  closeTab, closeTab, closeTab,
  moveTab, moveTab, moveTab,
  selectTab, selectTab,
  undoCloseTab,
  openTab,
];

function rand(min, max) {
  return min + Math.floor(Math.random() * (max - min + 1));
}
function pickOne(array) {
  return array[rand(0, array.length - 1)];
}
function pickOneTab() {
  var tab = pickOne(gBrowser.tabs);
  return [tab, Array.indexOf(gBrowser.tabs, tab)];
}
function nextSequence() {
  while (gBrowser.browsers.length > 1)
    gBrowser.removeCurrentTab();
  if (sequences-- <= 0) {
    allTabs.close();
    gBrowser.addTab();
    gBrowser.removeCurrentTab();
    finish();
    return;
  }
  closedTabs = 0;
  steps = rand(10, 20);
  var initialTabs = "";
  while (gBrowser.browsers.length < rand(3, 20)) {
    let tabChar = pickOne(chars);
    initialTabs += tabChar;
    gBrowser.addTab("data:text/plain," + tabChar);
  }
  history = [initialTabs];
  gBrowser.removeCurrentTab();
  next();
}
function next() {
  executeSoon(function () {
    is(allTabs.previews.length, gBrowser.browsers.length,
       history.join(", "));
    if (steps-- <= 0) {
      nextSequence();
      return;
    }
    var step;
    var rv;
    do {
      step = pickOne(allTabs.isOpen ? whenOpen : whenClosed);
      info(step.name);
      rv = step();
    } while (rv === false);
    history.push(step.name + (rv !== true && rv !== undefined ? " " + rv : ""));
  });
}

function openPanel() {
  if (allTabs.isOpen)
    return false;
  allTabs.panel.addEventListener("popupshown", function () {
    allTabs.panel.removeEventListener("popupshown", arguments.callee, false);
    next();
  }, false);
  allTabs.open();
  return true;
}

function closePanel() {
  allTabs.panel.addEventListener("popuphidden", function () {
    allTabs.panel.removeEventListener("popuphidden", arguments.callee, false);
    next();
  }, false);
  allTabs.close();
}

function closeTab() {
  if (gBrowser.browsers.length == 1)
    return false;
  var [tab, index] = pickOneTab();
  gBrowser.removeTab(tab);
  closedTabs++;
  next();
  return index;
}

function startSearch() {
  allTabs.filterField.value = pickOne(chars);
  info(allTabs.filterField.value);
  allTabs.filter();
  next();
  return allTabs.filterField.value;
}

function clearSearch() {
  if (!allTabs.filterField.value)
    return false;
  allTabs.filterField.value = "";
  allTabs.filter();
  next();
  return true;
}

function undoCloseTab() {
  if (!closedTabs)
    return false;
  window.undoCloseTab(0);
  closedTabs--;
  next();
  return true;
}

function selectTab() {
  var [tab, index] = pickOneTab();
  gBrowser.selectedTab = tab;
  next();
  return index;
}

function openTab() {
  BrowserOpenTab();
  next();
}

function moveTab() {
  if (gBrowser.browsers.length == 1)
    return false;
  var [tab, currentIndex] = pickOneTab();
  do {
    var [, newIndex] = pickOneTab();
  } while (newIndex == currentIndex);
  gBrowser.moveTabTo(tab, newIndex);
  next();
  return currentIndex + "->" + newIndex;
}
