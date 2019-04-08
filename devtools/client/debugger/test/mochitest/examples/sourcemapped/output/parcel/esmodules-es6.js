// modules are defined as an array
// [ module function, map of requires ]
//
// map of requires is short require name -> numeric require
//
// anything defined in a previous bundle is accessed via the
// orig method which is the require for previous bundles

// eslint-disable-next-line no-global-assign
parcelRequire = (function (modules, cache, entry, globalName) {
  // Save the require from previous bundle to this closure if any
  var previousRequire = typeof parcelRequire === 'function' && parcelRequire;
  var nodeRequire = typeof require === 'function' && require;

  function newRequire(name, jumped) {
    if (!cache[name]) {
      if (!modules[name]) {
        // if we cannot find the module within our internal map or
        // cache jump to the current global require ie. the last bundle
        // that was added to the page.
        var currentRequire = typeof parcelRequire === 'function' && parcelRequire;
        if (!jumped && currentRequire) {
          return currentRequire(name, true);
        }

        // If there are other bundles on this page the require from the
        // previous one is saved to 'previousRequire'. Repeat this as
        // many times as there are bundles until the module is found or
        // we exhaust the require chain.
        if (previousRequire) {
          return previousRequire(name, true);
        }

        // Try the node require function if it exists.
        if (nodeRequire && typeof name === 'string') {
          return nodeRequire(name);
        }

        var err = new Error('Cannot find module \'' + name + '\'');
        err.code = 'MODULE_NOT_FOUND';
        throw err;
      }

      localRequire.resolve = resolve;

      var module = cache[name] = new newRequire.Module(name);

      modules[name][0].call(module.exports, localRequire, module, module.exports, this);
    }

    return cache[name].exports;

    function localRequire(x){
      return newRequire(localRequire.resolve(x));
    }

    function resolve(x){
      return modules[name][1][x] || x;
    }
  }

  function Module(moduleName) {
    this.id = moduleName;
    this.bundle = newRequire;
    this.exports = {};
  }

  newRequire.isParcelRequire = true;
  newRequire.Module = Module;
  newRequire.modules = modules;
  newRequire.cache = cache;
  newRequire.parent = previousRequire;
  newRequire.register = function (id, exports) {
    modules[id] = [function (require, module) {
      module.exports = exports;
    }, {}];
  };

  for (var i = 0; i < entry.length; i++) {
    newRequire(entry[i]);
  }

  if (entry.length) {
    // Expose entry point to Node, AMD or browser globals
    // Based on https://github.com/ForbesLindesay/umd/blob/master/template.js
    var mainExports = newRequire(entry[entry.length - 1]);

    // CommonJS
    if (typeof exports === "object" && typeof module !== "undefined") {
      module.exports = mainExports;

    // RequireJS
    } else if (typeof define === "function" && define.amd) {
     define(function () {
       return mainExports;
     });

    // <script>
    } else if (globalName) {
      this[globalName] = mainExports;
    }
  }

  // Override the current require with this new one
  return newRequire;
})({"src/mod1.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = "a-default";
},{}],"src/mod2.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
const aNamed = exports.aNamed = "a-named";
},{}],"src/mod3.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
const original = exports.original = "an-original";
},{}],"src/mod4.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = "a-default";
const aNamed = exports.aNamed = "a-named";
},{}],"src/mod5.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = "a-default2";
},{}],"src/mod6.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
const aNamed2 = exports.aNamed2 = "a-named2";
},{}],"src/mod7.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
const original = exports.original = "an-original2";
},{}],"src/mod9.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = "a-default3";
},{}],"src/mod10.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
const aNamed3 = exports.aNamed3 = "a-named3";
},{}],"src/mod11.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
const original = exports.original = "an-original3";
},{}],"src/optimized-out.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = optimizedOut;
function optimizedOut() {}
},{}],"input.js":[function(require,module,exports) {
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = root;
exports.example = example;

var _mod = require("./src/mod1");

var _mod2 = _interopRequireDefault(_mod);

var _mod3 = require("./src/mod2");

var _mod4 = require("./src/mod3");

var _mod5 = require("./src/mod4");

var aNamespace = _interopRequireWildcard(_mod5);

var _mod6 = require("./src/mod5");

var _mod7 = _interopRequireDefault(_mod6);

var _mod8 = require("./src/mod6");

var _mod9 = require("./src/mod7");

var _mod10 = require("./src/mod9");

var _mod11 = _interopRequireDefault(_mod10);

var _mod12 = require("./src/mod10");

var _mod13 = require("./src/mod11");

var _optimizedOut = require("./src/optimized-out");

var _optimizedOut2 = _interopRequireDefault(_optimizedOut);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

(0, _optimizedOut2.default)();

function root() {
  console.log("pause here", root);

  console.log(_mod2.default);
  console.log(_mod4.original);
  console.log(_mod3.aNamed);
  console.log(_mod3.aNamed);
  console.log(aNamespace);

  try {
    // None of these are callable in this code, but we still want to make sure
    // they map properly even if the only reference is in a call expressions.
    console.log((0, _mod7.default)());
    console.log((0, _mod9.original)());
    console.log((0, _mod8.aNamed2)());
    console.log((0, _mod8.aNamed2)());

    console.log(new _mod11.default());
    console.log(new _mod13.original());
    console.log(new _mod12.aNamed3());
    console.log(new _mod12.aNamed3());
  } catch (e) {}
}

function example() {}
},{"./src/mod1":"src/mod1.js","./src/mod2":"src/mod2.js","./src/mod3":"src/mod3.js","./src/mod4":"src/mod4.js","./src/mod5":"src/mod5.js","./src/mod6":"src/mod6.js","./src/mod7":"src/mod7.js","./src/mod9":"src/mod9.js","./src/mod10":"src/mod10.js","./src/mod11":"src/mod11.js","./src/optimized-out":"src/optimized-out.js"}]},{},["input.js"], "parcelEsmodulesEs6")
//# sourceMappingURL=esmodules-es6.map
;parcelEsmodulesEs6 = parcelEsmodulesEs6.default;