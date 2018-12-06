setModuleResolveHook(function(module, specifier) {
    throw "Module '" + specifier + "' not found";
});
g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = () => null;");

let exc = "fail";
// This import fails because no such file exists, so we fire the onExceptionUnwind hook.
import("javascript: ").catch(e => { exc = e; });
drainJobQueue();
assertEq(exc, undefined);
