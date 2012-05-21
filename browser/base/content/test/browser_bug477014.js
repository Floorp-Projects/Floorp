/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// That's a gecko!
const iconURLSpec = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";
var testPage="data:text/plain,test bug 477014";

function test() {
  waitForExplicitFinish();

  var newWindow;
  var tabToDetach;

  function onPageShow(event) {
    // we get here if the test is executed before the pageshow
    // event for the window's first tab
    if (!tabToDetach ||
        tabToDetach.linkedBrowser.contentDocument != event.target)
      return;

    event.currentTarget.removeEventListener("pageshow", onPageShow, false);

    if (!newWindow) {
      // prepare the tab (set icon and busy state)
      // we have to set these only after onState* notification, otherwise
      // they're overriden
      setTimeout(function() {
        gBrowser.setIcon(tabToDetach, iconURLSpec);
        tabToDetach.setAttribute("busy", "true");

        // detach and set the listener on the new window
        newWindow = gBrowser.replaceTabWithWindow(tabToDetach);
        // wait for gBrowser to come along
        newWindow.addEventListener("load", function () {
          newWindow.removeEventListener("load", arguments.callee, false);
          newWindow.gBrowser.addEventListener("pageshow", onPageShow, false);
        }, false);
      }, 0);
      return;
    }

    is(newWindow.gBrowser.selectedTab.hasAttribute("busy"), true);
    is(newWindow.gBrowser.getIcon(), iconURLSpec);
    newWindow.close();
    finish();
  }

  gBrowser.addEventListener("pageshow", onPageShow, false);
  tabToDetach = gBrowser.addTab(testPage);
}
