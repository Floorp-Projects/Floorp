// Helpers for Media Source Extensions tests

function runWithMSE(testFunction) {
  addLoadEvent(() => SpecialPowers.pushPrefEnv({"set": [[ "media.mediasource.enabled", true ]]}, testFunction));
}

function fetchWithXHR(uri, onLoadFunction) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", uri, true);
  xhr.responseType = "blob";
  xhr.addEventListener("load", function (e) {
    is(xhr.status, 200, "fetchWithXHR load uri='" + uri + "' status=" + xhr.status);
    var rdr = new FileReader();
    rdr.addEventListener("load", function (e) {
      onLoadFunction(e.target.result);
    });
    rdr.readAsArrayBuffer(e.target.response);
  });
  xhr.send();
};
