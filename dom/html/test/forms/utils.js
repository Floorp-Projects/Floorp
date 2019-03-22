/**
 * Simulate the user clicks the reset button of the given date or time element.
 *
 * @param inputElement A date or time input element of default size.
 */
function simulateUserClicksResetButton(inputElement) {
  var inputRectangle = inputElement.getBoundingClientRect();
  const offsetX = inputRectangle.width - 15;
  const offsetY = inputRectangle.height / 2;

  synthesizeMouse(inputElement, offsetX, offsetY, {});
}

/**
 * @param navigator https://www.w3schools.com/jsref/obj_navigator.asp.
 * @return true, iff it's a desktop user agent.
 */
function isDesktopUserAgent(navigator) {
  return !/Mobile|Tablet/.test(navigator.userAgent);
}

