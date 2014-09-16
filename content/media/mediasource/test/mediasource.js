// Helpers for Media Source Extensions tests

function runWithMSE(testFunction) {
  function bootstrapTest() {
    var ms = new MediaSource();

    var el = document.createElement("video");
    el.src = URL.createObjectURL(ms);
    el.preload = "auto";

    document.body.appendChild(el);
    SimpleTest.registerCleanupFunction(function () {
      el.parentNode.removeChild(el);
    });

    testFunction(ms, el);
  }

  addLoadEvent(function () {
    SpecialPowers.pushPrefEnv({"set": [[ "media.mediasource.enabled", true ]]},
                              bootstrapTest);
  });
}

function fetchWithXHR(uri, onLoadFunction) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", uri, true);
  xhr.responseType = "arraybuffer";
  xhr.addEventListener("load", function () {
    is(xhr.status, 200, "fetchWithXHR load uri='" + uri + "' status=" + xhr.status);
    onLoadFunction(xhr.response);
  });
  xhr.send();
};
