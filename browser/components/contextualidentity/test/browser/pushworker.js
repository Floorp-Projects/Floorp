onpush = function() {
  fetch("http://example.com/browser/browser/components/contextualidentity/test/browser/pushserver.sjs",
        { method: "POST", body: "42" });
}
