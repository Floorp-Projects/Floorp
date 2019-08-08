# How to handle E10S in actors

In multi-process environments, most devtools actors are created and initialized in the child content process, to be able to access the resources they are exposing to the toolbox. But sometimes, these actors need to access things in the parent process too. Here's why and how.

{% hint style="error" %}

This documentation page is **deprecated**. `setupInParent` relies on the message manager which is being deprecated. Furthermore, communications between parent and content processes should be avoided for security reasons. If possible, the client should be responsible for calling actors both on the parent and content process.

This page will be removed when all actors relying on this API are removed.

{% endhint %}

## Use case and examples

Some actors need to exchange messages between the parent and the child process (typically when some components aren't available in the child process).

To that end, there's a parent/child setup mechanism at `DebuggerServer` level that can be used.

When the actor is loaded for the first time in the `DebuggerServer` running in the child process, it may decide to run a setup procedure to load a module in the parent process with which to communicate.

Example code for the actor running in the child process:

```
  const {DebuggerServer} = require("devtools/server/debugger-server");

  // Setup the child<->parent communication only if the actor module
  // is running in a child process.
  if (DebuggerServer.isInChildProcess) {
    setupChildProcess();
  }

  function setupChildProcess() {
    // `setupInParent`  is defined on DebuggerServerConnection,
    // your actor receives a reference to one instance in its constructor.
    conn.setupInParent({
      module: "devtools/server/actors/module-name",
      setupParent: "setupParentProcess"
    });
    // ...
  }
```

The `setupChildProcess` helper defined and used in the previous example uses the `DebuggerServerConnection.setupInParent` to run a given setup function in the parent process Debugger Server.

With this, the `DebuggerServer` running in the parent process will require the requested module and call its `setupParentProcess` function (which should be exported on the module).

The `setupParentProcess` function will receive a parameter that contains a reference to the **MessageManager** and a prefix that should be used to send/receive messages between the child and parent processes.

See below an example implementation of a `setupParent` function in the parent process:

```
exports.setupParentProcess = function setupParentProcess({ mm, prefix }) {
  // Start listening for messages from the actor in the child process.
  setMessageManager(mm);

  function handleChildRequest(msg) {
    switch (msg.json.method) {
      case "get":
        return doGetInParentProcess(msg.json.args[0]);
        break;
      case "list":
        return doListInParentProcess();
        break;
      default:
        console.error("Unknown method name", msg.json.method);
        throw new Error("Unknown method name");
    }
  }

  function setMessageManager(newMM) {
    if (mm) {
      // Remove listener from old message manager
      mm.removeMessageListener("debug:some-message-name", handleChildRequest);
    }
    // Switch to the new message manager for future use
    // Note: Make sure that any other functions also use the new reference.
    mm = newMM;
    if (mm) {
      // Add listener to new message manager
      mm.addMessageListener("debug:some-message-name", handleChildRequest);
    }
  }

  return {
    onBrowserSwap: setMessageManager,
    onDisconnected: () => setMessageManager(null),
  };
};
```

The server will call the `onDisconnected` method returned by the parent process setup flow to give the actor modules the chance to cleanup their handlers registered on the disconnected message manager.

The server will call the `onBrowserSwap` method returned by the parent process setup flow to notify actor modules when the message manager for the target frame has changed.  The parent process code should remove any message listeners from the previous message manager and add them to the new one.

## Summary of the setup flow

In the child process:

* The `DebuggerServer` loads an actor module,
* the actor module checks `DebuggerServer.isInChildProcess` to know whether it runs in a child process or not,
* the actor module then uses the `DebuggerServerConnection.setupInParent` helper to start setting up a parent-process counterpart,
* the `DebuggerServerConnection.setupInParent` helper asks the parent process to run the required module's setup function,
* the actor module uses the `DebuggerServerConnection.parentMessageManager.sendSyncMessage` and `DebuggerServerConnection.parentMessageManager.addMessageListener` helpers to send or listen to message.

In the parent process:

* The DebuggerServer receives the `DebuggerServerConnection.setupInParent` request,
* tries to load the required module,
* tries to call the `module[setupParent]` function with the frame message manager and the prefix as parameters `{ mm, prefix }`,
* the `setupParent` function then uses the mm to subscribe the message manager events,
* the `setupParent` function returns an object with `onDisconnected` and `onBrowserSwap` methods which the server can use to notify the module of various lifecycle events
