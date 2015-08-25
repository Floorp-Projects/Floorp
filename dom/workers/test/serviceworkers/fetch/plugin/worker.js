self.addEventListener("fetch", function(event) {
  var resource = event.request.url.split('/').pop();
  if (event.client) {
    event.client.postMessage({context: event.request.context,
                              resource: resource});
  }
});
