function test()
{
  waitForExplicitFinish();
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
