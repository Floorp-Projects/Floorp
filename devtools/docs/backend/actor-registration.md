# How to register an actor

## Tab actors vs. global actors

Tab actors are the most common types of actors. That's the type of actors you will most probably be adding.

Tab actors target a document, this could be a tab in Firefox or a remote document in Firefox for Android.

Global actors however are for the rest, for things not related to any particular document but instead for things global to the whole Firefox/Chrome/Safari intance the toolbox is connected to (e.g. the preference actor).

## The DebuggerServer.registerModule function

To register a tab actor:

```
DebuggerServer.registerModule("devtools/server/actors/webconsole", {
  prefix: "console",
  constructor: "WebConsoleActor",
  type: { tab: true }
});
```

To register a global actor:

```
DebuggerServer.registerModule("devtools/server/actors/addons", {
  prefix: "addons",
  constructor: "AddonsActor",
  type: { global: true }
});
```

If you are adding a new built-in devtools actor, you should be registering it using `DebuggerServer.registerModule` in `_addBrowserActors` or `addTabActors` in `/devtools/server/main.js`.

If you are adding a new actor from an add-on, you should call `DebuggerServer.registerModule` directly from your add-on code.

## A note about lazy registration

The `DebuggerServer` loads and creates all of the actors lazily to keep the initial memory usage down (which is extremely important on lower end devices).

It becomes especially important when debugging pages with e10s when there are more than one process, because that's when we need to spawn a `DebuggerServer` per process (it may not be immediately obvious that the server in the main process is mostly only here for piping messages to the actors in the child process).
