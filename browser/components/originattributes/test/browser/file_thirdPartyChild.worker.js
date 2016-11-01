var xhr = new XMLHttpRequest();
xhr.open("GET", "http://example.net/browser/browser/components/originattributes/test/browser/file_thirdPartyChild.worker.xhr.html", true);
xhr.send();

fetch("http://example.net/browser/browser/components/originattributes/test/browser/file_thirdPartyChild.worker.fetch.html", {cache: "force-cache"} );
var myRequest = new Request("http://example.net/browser/browser/components/originattributes/test/browser/file_thirdPartyChild.worker.request.html");
fetch(myRequest, {cache: "force-cache"} );

self.importScripts("http://example.net/browser/browser/components/originattributes/test/browser/file_thirdPartyChild.import.js");
