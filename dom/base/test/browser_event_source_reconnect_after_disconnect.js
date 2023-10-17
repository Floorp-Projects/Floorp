var { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function eventSourcePageHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);

  // An HTML page that sets up the EventSource
  let pageContent = `
    <html>
      <body>
        <script>
          let es = new EventSource("/eventstream");
          es.onopen = function() {
            console.log("send es_open");
            window.dispatchEvent(new CustomEvent('es-open'));
          };
          es.onmessage = function(err) {
            console.log("send es_message");
            window.dispatchEvent(new CustomEvent('es-message'));
          };
          es.onerror = function(err) {
            console.log("send es_error");
            window.dispatchEvent(new CustomEvent('es-error'));
          
        };
        </script>
      </body>
    </html>`;

  response.write(pageContent);
}

const server = new HttpServer();
server.start(-1);
const SERVER_PORT = server.identity.primaryPort;

registerCleanupFunction(async () => {
  await server.stop();
});

function eventStreamHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/event-stream", false);
  response.write("retry: 500\n");
}

server.registerPathHandler("/page", eventSourcePageHandler);
server.registerPathHandler("/eventstream", eventStreamHandler);

add_task(async function testReconnectAfterDisconnect() {
  info("Connect to the server to retrieve the EventSource doc");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `http://localhost:${server.identity.primaryPort}/page`
  );

  let browser = tab.linkedBrowser;

  // Define a function to handle events in the content process
  async function contentEventHandler() {
    return new Promise(resolve => {
      content.addEventListener(
        "es-open",
        function () {
          resolve("es-open");
        },
        { once: true }
      );

      content.addEventListener(
        "es-error",
        function () {
          resolve("es-error");
        },
        { once: true }
      );
    });
  }

  // Execute the event handler in the content process
  let eventType = await SpecialPowers.spawn(browser, [], contentEventHandler);
  Assert.equal(eventType, "es-open", "EventSource opened successfully");

  // Stop the server; we expect an error
  await server.stop();

  eventType = await SpecialPowers.spawn(browser, [], contentEventHandler);
  Assert.equal(
    eventType,
    "es-error",
    "EventSource encountered an error after server close"
  );

  // Restart the server; the eventSource should automatically reconnect
  server.start(SERVER_PORT);

  eventType = await SpecialPowers.spawn(browser, [], contentEventHandler);
  Assert.equal(
    eventType,
    "es-open",
    "EventSource opened successfully after server restart"
  );

  BrowserTestUtils.removeTab(tab);
});
