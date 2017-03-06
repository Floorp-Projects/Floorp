
var WebVRHelpers = (function() {
"use strict";

var RequestPresentOnVRDisplay = function(vrDisplay, vrLayers, callback) {
  if (callback) {
    callback();
  }

  return vrDisplay.requestPresent(vrLayers);
};

var API = {
  RequestPresentOnVRDisplay: RequestPresentOnVRDisplay,

  none: false
};

return API;

}());