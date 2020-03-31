# Client API

DevTools has a client module that allows applications to be written that debug or inspect web pages using the [Remote Debugging Protocol](protocol.md).

## Starting communication

In order to communicate, a client and a server instance must be created and a protocol connection must be established. The connection can be either over a TCP socket or an nsIPipe. The `start` function displayed below establishes an nsIPipe-backed connection:

```javascript
const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");

function start() {
  // Start the server.
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  // Listen to an nsIPipe
  let transport = DevToolsServer.connectPipe();

  // Start the client.
  client = new DevToolsClient(transport);

  client.connect((type, traits) => {
    // Now the client is connected to the server.
    debugTab();
  });
}
```

If a TCP socket is required, the function should be split in two parts, a server-side and a client-side, like this:

```javascript
const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");

function startServer() {
  // Start the server.
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  // For an nsIServerSocket we do this:
  DevToolsServer.openListener(2929); // A connection on port 2929.
}

async function startClient() {
  let transport = await DevToolsClient.socketConnect({ host: "localhost", port: 2929 });

  // Start the client.
  client = new DevToolsClient(transport);

  client.connect((type, traits) => {
    // Now the client is connected to the server.
    debugTab();
  });
}
```

## Shutting down

When the application is finished, it has to notify the client to shut down the protocol connection. This makes sure that memory leaks are avoided and the server is terminated in an orderly fashion. Shutting down is as simple as it gets:

```javascript
function shutdown() {
  client.close();
}
```

## Attaching to a browser tab

Attaching to a browser tab requires enumerating the available tabs and attaching to one:

```javascript
function attachToTab() {
  // Get the list of tabs to find the one to attach to.
  client.mainRoot.listTabs().then(tabs => {
    // Find the active tab.
    let targetFront = tabs.find(tab => tab.selected);

    // Attach to the tab.
    targetFront.attach().then(() => {
      // Now the targetFront is ready and can be used.

      // Attach listeners for client events.
      targetFront.on("tabNavigated", onTab);
    });
  });
}
```

The devtools client will send event notifications for a number of events the application may be interested in. These events include state changes in the debugger, like pausing and resuming, stack frames or source scripts being ready for retrieval, etc.

## Handling location changes

When the user navigates away from a page, a `tabNavigated` event will be fired. The proper way to handle this event is to detach from the previous thread and tab and attach to the new ones:

```javascript
async function onTab() {
  // Detach from the previous thread.
  await client.activeThread.detach();
  // Detach from the previous tab.
  await targetFront.detach();
  // Start debugging the new tab.
  start();
}
```

## Debugging JavaScript running in a browser tab

Once the application is attached to a tab, it can attach to its thread in order to interact with the JavaScript debugger:

```javascript
// Assuming the application is already attached to the tab, and response is the first
// argument of the attachTarget callback.

client.attachThread(response.threadActor).then(function(threadFront) {
  if (!threadFront) {
    return;
  }

  // Attach listeners for thread events.
  threadFront.on("paused", onPause);
  threadFront.on("resumed", fooListener);
  threadFront.on("detached", fooListener);

  // Resume the thread.
  threadFront.resume();

  // Debugger is now ready and debuggee is running.
});
```

## Debugger application example

Here is the source code for a complete debugger application:

```javascript
/*
 * Debugger API demo.
 */
const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");

let client;
let threadFront;

function startDebugger() {
  // Start the server.
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  // Listen to an nsIPipe
  let transport = DevToolsServer.connectPipe();
  // For an nsIServerSocket we do this:
  // DevToolsServer.openListener(port);
  // ...and this at the client:
  // let transport = debuggerSocketConnect(host, port);

  // Start the client.
  client = new DevToolsClient(transport);
  client.connect((type, traits) => {
    // Now the client is connected to the server.
    debugTab();
  });
}

function shutdownDebugger() {
  client.close();
}

/**
 * Start debugging the current tab.
 */
function debugTab() {
  // Get the list of tabs to find the one to attach to.
  client.mainRoot.listTabs().then(tabs => {
    // Find the active tab.
    let targetFront = tabs.find(tab => tab.selected);
    // Attach to the tab.
    targetFront.attach().then(() => {
      // Attach to the thread (context).
      targetFront.attachThread().then((threadFront) => {
        // Attach listeners for thread events.
        threadFront.on("paused", onPause);
        threadFront.on("resumed", fooListener);
        threadFront.on("detached", fooListener);

        // Resume the thread.
        threadFront.resume();
        // Debugger is now ready and debuggee is running.
      });
    });
  });
}

/**
 * Handler for location changes.
 */
function onTab() {
  // Detach from the previous thread.
  client.activeThread.detach().then(() => {
    // Detach from the previous tab.
    client.detach().then(() => {
      // Start debugging the new tab.
      debugTab();
    });
  });
}

/**
 * Helper function to inspect the provided frame.
 */
function inspectFrame(frame) {
  // Get the "this" object.
  if (frame["this"]) {
    getObjectProperties(frame["this"]);
  }

  // Add "arguments".
  if (frame.arguments && frame.arguments.length > 0) {
    // frame.arguments is a regular Array.
    dump("frame.arguments: " + frame.arguments.toSource() + "\n");

    // Add variables for every argument.
    let objClient = client.activeThread.pauseGrip(frame.callee);
    objClient.getSignature(response => {
      for (let i = 0; i < response.parameters.length; i++) {
        let name = response.parameters[i];
        let value = frame.arguments[i];

        if (typeof value == "object" && value.type == "object") {
          getObjectProperties(value);
        }
      }
    });
  }
}

/**
 * Helper function that retrieves the specified object's properties.
 */
function getObjectProperties(object) {
  let thisClient = client.activeThread.pauseGrip(object);
  thisClient.getPrototypeAndProperties(response => {
    // Get prototype as a protocol-specified grip.
    if (response.prototype.type != "null") {
      dump("__proto__: " + response.prototype.toSource() + "\n");
    }

    // Get the rest of the object's own properties as protocol-specified grips.
    for (let prop of Object.keys(response.ownProperties)) {
      dump(prop + ": " + response.ownProperties[prop].toSource() + "\n");
    }
  });
}

/**
 * Generic event listener.
 */
function fooListener(event) {
  dump(event + "\n");
}

// Run the program.
startDebugger();

// Execute the following line to stop the program.
//shutdownDebugger();
```
