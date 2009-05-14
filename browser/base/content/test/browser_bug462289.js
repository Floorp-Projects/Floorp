var tab1, tab2;

function test() {
  waitForExplicitFinish();

  tab1 = gBrowser.addTab();
  tab2 = gBrowser.addTab();

  EventUtils.synthesizeMouse(tab1, 2, 2, {});
  setTimeout(step2, 0);
}

function step2()
{
  isnot(document.activeElement, tab1, "mouse on tab not activeElement");

  EventUtils.synthesizeMouse(tab1, 2, 2, {});
  setTimeout(step3, 0);
}

function step3()
{
  isnot(document.activeElement, tab1, "mouse on tab again activeElement");

  EventUtils.synthesizeMouse(tab1, 2, 2, {button: 1, type: "mousedown"});
  EventUtils.synthesizeMouse(tab1, 0, 0, {button: 1, type: "mouseout"});
  setTimeout(step4, 0);
}

function step4()
{
  isnot(document.activeElement, tab1, "tab not focused via middle click");

  document.getElementById("searchbar").focus();
  EventUtils.synthesizeKey("VK_TAB", { });
  is(document.activeElement, tab1, "tab key to tab activeElement");

  EventUtils.synthesizeMouse(tab1, 2, 2, {});
  setTimeout(step5, 0);
}

function step5()
{
  is(document.activeElement, tab1, "mouse on tab while focused still activeElement");

  EventUtils.synthesizeMouse(tab2, 2, 2, {});
  setTimeout(step6, 0);
}

function step6()
{
  // The tabbox selects a tab within a setTimeout in a bubbling mousedown event
  // listener, and focuses the current tab if another tab previously had focus
  is(document.activeElement, tab2, "mouse on another tab while focused still activeElement");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);

  finish();
}
