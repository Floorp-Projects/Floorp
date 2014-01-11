/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function is_hidden(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding","closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(!is_hidden(element), msg);
}

function waitForElementToBeVisible(element, nextTest, msg) {
  waitForCondition(() => !is_hidden(element),
                   () => {
                     ok(true, msg);
                     nextTest();
                   },
                   "Timeout waiting for visibility: " + msg);
}

function waitForPopupAtAnchor(popup, anchorNode, nextTest, msg) {
  waitForCondition(() => popup.popupBoxObject.anchorNode == anchorNode,
                   () => {
                     ok(true, msg);
                     is_element_visible(popup);
                     nextTest();
                   },
                   "Timeout waiting for popup at anchor: " + msg);
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg);
}
