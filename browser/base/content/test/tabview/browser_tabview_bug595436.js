/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  
  window.addEventListener('tabviewshown', onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener('tabviewshown', onTabViewWindowLoaded, false);
  
  let contentWindow = document.getElementById('tab-view').contentWindow;
  let search = contentWindow.document.getElementById('search');
  let searchButton = contentWindow.document.getElementById('searchbutton');
  
  let isSearchEnabled = function () {
    return 'none' != search.style.display;
  }
  
  let assertSearchIsEnabled = function () {
    ok(isSearchEnabled(), 'search is enabled');
  }
  
  let assertSearchIsDisabled = function () {
    ok(!isSearchEnabled(), 'search is disabled');
  }
  
  let testSearchInitiatedByKeyPress = function () {
    EventUtils.synthesizeKey('a', {});
    assertSearchIsEnabled();
    
    EventUtils.synthesizeKey('VK_BACK_SPACE', {});
    assertSearchIsDisabled();
  }
  
  let testSearchInitiatedByMouseClick = function () {
    EventUtils.sendMouseEvent({type: 'mousedown'}, searchButton, contentWindow);
    assertSearchIsEnabled();
    
    EventUtils.synthesizeKey('a', {});
    EventUtils.synthesizeKey('VK_BACK_SPACE', {});
    EventUtils.synthesizeKey('VK_BACK_SPACE', {});
    assertSearchIsEnabled();
    
    EventUtils.synthesizeKey('VK_ESCAPE', {});
    assertSearchIsDisabled();
  }
  
  let finishTest = function () {
    let onTabViewHidden = function () {
      window.removeEventListener('tabviewhidden', onTabViewHidden, false);
      finish();
    }
    
    window.addEventListener('tabviewhidden', onTabViewHidden, false);
    TabView.hide();
  }
  
  assertSearchIsDisabled();
  
  testSearchInitiatedByKeyPress();
  testSearchInitiatedByMouseClick();
  
  finishTest();
}
