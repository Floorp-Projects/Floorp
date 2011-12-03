function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();
  var frame = document.getElementById("customizeToolbarSheetIFrame");
  frame.addEventListener("load", testCustomizeFrameLoadedPre, true);

  document.getElementById("cmd_CustomizeToolbars").doCommand();
}

function testCustomizeFrameLoadedPre(){
  // This load listener can be called before
  // customizeToolbarSheet.xul's, which would cause the test
  // to fail. Use executeSoon to delay running the test until
  // event dispatch is over (all load event listeners have run).
  executeSoon(testCustomizeFrameLoaded);
}

function testCustomizeFrameLoaded()
{
  var panel = document.getElementById("customizeToolbarSheetPopup");
  panel.addEventListener("popuphidden", testCustomizePopupHidden, false);

  var frame = document.getElementById("customizeToolbarSheetIFrame");
  frame.removeEventListener("load", testCustomizeFrameLoadedPre, true);

  if (navigator.platform.indexOf("Mac") == -1) {
    var menu = document.getElementById("bookmarksMenuPopup");
    ok("result" in menu, "menu has binding");
  }

  var framedoc = document.getElementById("customizeToolbarSheetIFrame").contentDocument;

  var panelX = panel.boxObject.screenX;
  var iconModeList = framedoc.getElementById("modelist");
  iconModeList.addEventListener("popupshown", function (e) {
    iconModeList.removeEventListener("popupshown", arguments.callee, false);
    SimpleTest.executeSoon(function () {
      is(panel.boxObject.screenX, panelX, "toolbar customization panel shouldn't move when the iconmode menulist is opened");
      iconModeList.open = false;
    
      var b = framedoc.getElementById("donebutton");
      b.focus();
      b.doCommand();
    });
  }, false);
  iconModeList.open = true;
}
  
function testCustomizePopupHidden(e)
{
  var panel = document.getElementById("customizeToolbarSheetPopup");
  if (e.target != panel)
    return;

  panel.removeEventListener("popuphidden", testCustomizePopupHidden, false);
  is(document.activeElement, document.documentElement, "focus after customize done");

  finish();
}
