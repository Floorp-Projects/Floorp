// A simple worker script that forward intercepted url to the controlled window.

function responseMsg(msg) {
  self.clients
    .matchAll({
      includeUncontrolled: true,
      type: "window",
    })
    .then(clients => {
      if (clients && clients.length) {
        clients[0].postMessage(msg);
      }
    });
}

onfetch = function (e) {
  responseMsg(e.request.url);
};
