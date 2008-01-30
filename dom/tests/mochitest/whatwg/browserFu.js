/**
 * File which includes stuff for Mozilla-specific checks which shouldn't happen
 * in other browsers but which we wish to test.
 */

var isMozilla = navigator.product === "Gecko" && "buildID" in navigator;
