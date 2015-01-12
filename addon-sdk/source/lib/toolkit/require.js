const make = (exports, rootURI, components) => {
  const { Loader: { Loader, Require, Module, main } } =
        components.utils.import(rootURI + "toolkit/loader.js", {});

  const loader = Loader({
    id: "toolkit/require",
    rootURI: rootURI,
    isNative: true,
    paths: {
     "": rootURI,
     "devtools/": "resource://gre/modules/devtools/"
    }
  });

  // Below we define `require` & `require.resolve` that resolve passed
  // module id relative to the caller URI. This is not perfect but good
  // enough for common case & there is always an option to pass absolute
  // id when that
  // but presumably well enough to cover

  const require = id => {
    const requirerURI = components.stack.caller.filename;
    const requirer = Module(requirerURI, requirerURI);
    return Require(loader, requirer)(id);
  };

  require.resolve = id => {
    const requirerURI = components.stack.caller.filename;
    const requirer = Module(requirerURI, requirerURI);
    return Require(loader, requirer).resolve(id);
  };

  exports.require = require;
}

// If loaded in the context of commonjs module, reload as JSM into an
// exports object.
if (typeof(require) === "function" && typeof(module) === "object") {
  require("chrome").Cu.import(module.uri, module.exports);
}
// If loaded in the context of JSM make a loader & require and define
// new symbols as exported ones.
else if (typeof(__URI__) === "string" && this["Components"]) {
  const builtin = Object.keys(this);
  const uri = __URI__.replace("toolkit/require.js", "");
  make(this, uri, this["Components"]);

  this.EXPORTED_SYMBOLS = Object.
                            keys(this).
                            filter($ => builtin.indexOf($) < 0);
}
else {
  throw Error("Loading require.js in this environment isn't supported")
}
