/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file expectes the SpecialPowers to be available in the scope
// it is loaded into.
/* global SpecialPowers */

export function ModuleLoader(base, depth, proto) {
  const modules = {};

  const principal = SpecialPowers.wrap(document).nodePrincipal;

  const sharedGlobalSandbox = SpecialPowers.Cu.Sandbox(principal, {
    invisibleToDebugger: true,
    sandboxName: "FS Module Loader",
    sandboxPrototype: proto,
    wantComponents: false,
    wantGlobalProperties: [],
    wantXrays: false,
  });

  const require = async function (id) {
    if (modules[id]) {
      return modules[id].exported_symbols;
    }

    const url = new URL(depth + id, base);

    const module = Object.create(null, {
      exported_symbols: {
        configurable: false,
        enumerable: true,
        value: Object.create(null),
        writable: true,
      },
    });

    modules[id] = module;

    const properties = {
      require_module: require,
      exported_symbols: module.exported_symbols,
    };

    // Create a new object in this sandbox, that will be used as the scope
    // object for this particular module.
    const sandbox = sharedGlobalSandbox.Object();
    Object.assign(sandbox, properties);

    SpecialPowers.Services.scriptloader.loadSubScript(url.href, sandbox);

    return module.exported_symbols;
  };

  const returnObj = {
    require: {
      enumerable: true,
      value: require,
    },
  };

  return Object.create(null, returnObj);
}
