/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function menuTest()
{
  gBrowser.selectedBrowser.removeEventListener("load", menuTest, true);

  let menuContents = [
    "menu_pageinspect",
    "webConsole",
    "menu_scratchpad",
    "menu_pageSource",
    "javascriptConsole"
  ];

  let menu = document.getElementById("webDeveloperMenu");
  ok(menu, "we have the menu");

  let popup = menu.firstChild;
  is(popup.id, "menuWebDeveloperPopup", "menu first child is menuWebDeveloperPopup");

  is(popup.childNodes.length, menuContents.length, "popup childNodes.length matches");

  for(let a = 0; a < popup.children.length; a++) {
    isnot(menuContents.indexOf(popup.children[a].id), -1, "menuitem " + popup.children[a].id + " in popup");
  };

  gBrowser.removeCurrentTab();
  finish();  
}

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", menuTest, true);

  content.location = "data:text/html,<title>Web Developer Menu Test</title>" +
    "<p>testing the Web Developer Menu";
}

