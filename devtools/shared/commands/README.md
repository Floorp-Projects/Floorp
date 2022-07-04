# Commands

Commands are singletons, which can be easily used by any frontend code.
They are meant to be exposed widely to the frontend so that any code can easily call any of their methods.

Commands classes expose static methods, which:
* route to the right Front/Actor's method
* handle backward compatibility
* map to many target's actor if needed

These classes are instantiated once per descriptor
and may have inner state, emit events, fire callbacks,...

A transient backward compat need, required by Fission refactorings will be to have some code checking a trait, and either:
* call a single method on a parent process actor (like BreakpointListActor.setBreakpoint)
* otherwise, call a method on each target's scoped actor (like ThreadActor.setBreakpoint, that, for each available target)

Without such layer, we would have to put such code here and there in the frontend code.
This will be harder to remove later, once we get rid of old pre-fission-refactoring codepaths.

This layer already exists in some panels, but we are using slightly different names and practices:
* Debugger uses "client" (devtools/client/debugger/src/client/) and "commands" (devtools/client/debugger/src/client/firefox/commands.js)
  Debugger's commands already bundle the code to dispatch an action to many target's actor.
  They also contain some backward compat code.
  Today, we pass around a `client` object via thunkArgs, which is mapped to commands.js,
  instead we could pass a debugger command object.
* Network Monitor uses "connector" (devtools/client/netmonitor/src/connector)
  Connectors also bundles backward compat and dispatch to many target's actor.
  Today, we pass the `connector` to all middlewares from configureStore,
  we could instead pass the netmonitor command object.
* Web Console has:
  * devtools/client/webconsole/actions/input.js:handleHelperResult(), where we have to put some code, which is a duplicate of Netmonitor Connector,
    and could be shared via a netmonitor command class.
* Inspector is probably the panel doing the most dispatch to many target's actor.
  Codes using getAllInspectorFronts could all be migrated to an inspector command class:
  https://searchfox.org/mozilla-central/search?q=symbol:%23getAllInspectorFronts&redirect=false
  and simplify a bit the frontend.
  It is also one panel, which still register listener to each target's inspector/walker fronts.
  Because inspector isn't using resources.
  But this work, registering listeners for each target might be done by such layer and translate the many actor's event into a unified one.

Last, but not least, this layer may allow us to slowly get rid of protocol.js.
Command classes aren't Fronts, nor are they particularly connected to protocol.js.
If we make it so that all the Frontend code using Fronts uses Commands instead, we might more easily get away from protocol.js.
