function test() {
  waitForExplicitFinish();

  openToolbarCustomizationUI(customizationWindowLoaded);
}

function customizationWindowLoaded(win) {
  let x = win.screenX;
  let iconModeList = win.document.getElementById("modelist");

  iconModeList.addEventListener("popupshown", function popupshown() {
    iconModeList.removeEventListener("popupshown", popupshown, false);

    executeSoon(function () {
      is(win.screenX, x,
         "toolbar customization window shouldn't move when the iconmode menulist is opened");
      iconModeList.open = false;
    
      closeToolbarCustomizationUI(finish);
    });
  }, false);

  iconModeList.open = true;
}
