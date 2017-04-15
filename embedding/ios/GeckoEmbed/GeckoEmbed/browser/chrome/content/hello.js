Components.utils.import("resource://gre/modules/Services.jsm");
addEventListener("DOMContentLoaded", function loaded() {
  removeEventListener("DOMContentLoaded", loaded);
  var b = document.getElementById("browser");
  Services.obs.notifyObservers(b.docShell,
                               "geckoembed-browser-loaded");
  b.loadURI("http://people.mozilla.org/~tmielczarek/iosstart.html");
});
