/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let temp = {}
Cu.import("resource:///modules/devtools/gDevTools.jsm", temp);
let DevTools = temp.DevTools;

Cu.import("resource:///modules/devtools/Toolbox.jsm", temp);
let Toolbox = temp.Toolbox;

Cu.import("resource:///modules/devtools/Target.jsm", temp);
let TargetFactory = temp.TargetFactory;

let toolbox, target;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target)
             .then(testBottomHost, console.error)
             .then(null, console.error);
  }, true);

  content.location = "data:text/html,test for opening toolbox in different hosts";
}

function testBottomHost(aToolbox)
{
  toolbox = aToolbox;

  checkHostType(Toolbox.HostType.BOTTOM);

  // test UI presence
  let nbox = gBrowser.getNotificationBox();
  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  ok(iframe, "toolbox bottom iframe exists");

  checkToolboxLoaded(iframe);

  toolbox.switchHost(Toolbox.HostType.SIDE).then(testSidebarHost);
}

function testSidebarHost()
{
  checkHostType(Toolbox.HostType.SIDE);

  // test UI presence
  let nbox = gBrowser.getNotificationBox();
  let bottom = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);

  toolbox.switchHost(Toolbox.HostType.WINDOW).then(testWindowHost);
}

function testWindowHost()
{
  checkHostType(Toolbox.HostType.WINDOW);

  let nbox = gBrowser.getNotificationBox();
  let sidebar = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  ok(!sidebar, "toolbox sidebar iframe doesn't exist");

  let win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");

  let iframe = win.document.getElementById("toolbox-iframe");
  checkToolboxLoaded(iframe);

  testToolSelect();
}

function testToolSelect()
{
  // make sure we can load a tool after switching hosts
  toolbox.selectTool("inspector").then(testDestroy);
}

function testDestroy()
{
  toolbox.destroy().then(function() {
    target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target).then(testRememberHost);
  });
}

function testRememberHost(aToolbox)
{
  toolbox = aToolbox;
  // last host was the window - make sure it's the same when re-opening
  is(toolbox.hostType, Toolbox.HostType.WINDOW, "host remembered");

  let win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");

  cleanup();
}

function checkHostType(hostType)
{
  is(toolbox.hostType, hostType, "host type is " + hostType);

  let pref = Services.prefs.getCharPref("devtools.toolbox.host");
  is(pref, hostType, "host pref is " + hostType);
}

function checkToolboxLoaded(iframe)
{
  let tabs = iframe.contentDocument.getElementById("toolbox-tabs");
  ok(tabs, "toolbox UI has been loaded into iframe");
}

function cleanup()
{
  Services.prefs.setCharPref("devtools.toolbox.host", Toolbox.HostType.BOTTOM);

  toolbox.destroy().then(function() {
    DevTools = Toolbox = toolbox = target = null;
    gBrowser.removeCurrentTab();
    finish();
  });
 }
