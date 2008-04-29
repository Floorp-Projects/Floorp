function test()
{
  waitForExplicitFinish();
  var panel = document.getElementById("customizeToolbarSheetPopup");
  panel.addEventListener("popupshown", testCustomizePopupShown, false);
  document.getElementById("cmd_CustomizeToolbars").doCommand();
}

function testCustomizePopupShown()
{
  var panel = document.getElementById("customizeToolbarSheetPopup");
  panel.removeEventListener("popupshown", testCustomizePopupShown, false);
  panel.addEventListener("popuphidden", testCustomizePopupHidden, false);

  var frame = document.getElementById("customizeToolbarSheetIFrame").contentDocument;
  frame.addEventListener("load", testCustomizeFrameLoaded, true);
}

function testCustomizeFrameLoaded()
{
  var frame = document.getElementById("customizeToolbarSheetIFrame");
  frame.removeEventListener("load", testCustomizeFrameLoaded, true);

  var menu = document.getElementById("bookmarksMenuPopup");
  ok("getResult" in menu, "menu has binding");

  var framedoc = document.getElementById("customizeToolbarSheetIFrame").contentDocument;
  var b = framedoc.getElementById("donebutton");

  framedoc.getElementById("donebutton").doCommand();
}
  
function testCustomizePopupHidden()
{
  var panel = document.getElementById("customizeToolbarSheetPopup");
  panel.removeEventListener("popuphidden", testCustomizePopupHidden, false);
  finish();
}
