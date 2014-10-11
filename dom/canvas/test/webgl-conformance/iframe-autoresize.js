IFrameAutoresize = (function(){
  var IFRAME_BODY_MARGIN = 8;
  var UPDATE_INTERVAL = 100; // ms
  var MIN_HEIGHT = 100;

  function Start(elem, hasMargin) {
    var fnResize = function() {
      var frameBody = elem.contentWindow.document.body;
      if (frameBody) {
        var frameHeight = frameBody.scrollHeight;
        if (hasMargin) {
          frameHeight += 2*IFRAME_BODY_MARGIN;
        }

        elem.height = Math.max(MIN_HEIGHT, frameHeight);
      }

      setTimeout(fnResize, UPDATE_INTERVAL);
    }
    fnResize();
  }

  function StartById(id, hasMargin) {
    var elem = document.getElementById(id);
    Start(elem, hasMargin);
  }

  return {
    Start: Start,
    StartById: StartById,
  };
})();
