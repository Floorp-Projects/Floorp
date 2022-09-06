/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function ModuleLoader(base, depth, proto) {
  const modules = {};

  const require = async function(id) {
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

    const xhr = new XMLHttpRequest();
    xhr.open("GET", url.href, false);
    xhr.responseType = "text";
    xhr.send();

    let source = xhr.responseText;

    let code = new Function(
      "require_module",
      "exported_symbols",
      `eval(arguments[2] + "\\n//# sourceURL=" + arguments[3] + "\\n")`
    );
    code(require, module.exported_symbols, source, url.href);

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
