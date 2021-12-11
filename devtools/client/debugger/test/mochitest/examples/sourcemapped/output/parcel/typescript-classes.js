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
})({"src/mod.ts":[function(require,module,exports) {
"use strict";

exports.__esModule = true;
function decoratorFactory(opts) {
    return function decorator(target) {
        return target;
    };
}
exports.decoratorFactory = decoratorFactory;
function def() {}
exports["default"] = def;
},{}],"input.ts":[function(require,module,exports) {
"use strict";
// This file essentially reproduces an example Angular component to map testing,
// among other typescript edge cases.

var __extends = this && this.__extends || function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function (d, b) {
            d.__proto__ = b;
        } || function (d, b) {
            for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
        };
        return extendStatics(d, b);
    };
    return function (d, b) {
        extendStatics(d, b);
        function __() {
            this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
}();
var __decorate = this && this.__decorate || function (decorators, target, key, desc) {
    var c = arguments.length,
        r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc,
        d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __importStar = this && this.__importStar || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
exports.__esModule = true;
var mod_ts_1 = require("./src/mod.ts");
var ns = __importStar(require("./src/mod.ts"));
var AppComponent = /** @class */function () {
    function AppComponent() {
        this.title = 'app';
    }
    AppComponent = __decorate([mod_ts_1.decoratorFactory({
        selector: 'app-root'
    })], AppComponent);
    return AppComponent;
}();
exports.AppComponent = AppComponent;
var fn = function (arg) {
    console.log("here");
};
fn("arg");
// Un-decorated exported classes present a mapping challege because
// the class name is mapped to an unhelpful export assignment.
var ExportedOther = /** @class */function () {
    function ExportedOther() {
        this.title = 'app';
    }
    return ExportedOther;
}();
exports.ExportedOther = ExportedOther;
var AnotherThing = /** @class */function () {
    function AnotherThing() {
        this.prop = 4;
    }
    return AnotherThing;
}();
var ExpressionClass = /** @class */function () {
    function Foo() {
        this.prop = 4;
    }
    return Foo;
}();
var SubDecl = /** @class */function (_super) {
    __extends(SubDecl, _super);
    function SubDecl() {
        var _this = _super !== null && _super.apply(this, arguments) || this;
        _this.prop = 4;
        return _this;
    }
    return SubDecl;
}(AnotherThing);
var SubVar = /** @class */function (_super) {
    __extends(SubExpr, _super);
    function SubExpr() {
        var _this = _super !== null && _super.apply(this, arguments) || this;
        _this.prop = 4;
        return _this;
    }
    return SubExpr;
}(AnotherThing);
ns;
function default_1() {
    // This file is specifically for testing the mappings of classes and things
    // above, which means we don't want to include _other_ references to then.
    // To avoid having them be optimized out, we include a no-op eval.
    eval("");
    console.log("pause here");
}
exports["default"] = default_1;
},{"./src/mod.ts":"src/mod.ts"}]},{},["input.ts"], "parcelTypescriptClasses")
//# sourceMappingURL=typescript-classes.map
;parcelTypescriptClasses = parcelTypescriptClasses.default;