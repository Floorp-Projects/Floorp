var webpack3TypescriptClasses =
/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, {
/******/ 				configurable: false,
/******/ 				enumerable: true,
/******/ 				get: getter
/******/ 			});
/******/ 		}
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 1);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";

exports.__esModule = true;
function decoratorFactory(opts) {
    return function decorator(target) {
        return target;
    };
}
exports.decoratorFactory = decoratorFactory;
function def() { }
exports["default"] = def;


/***/ }),
/* 1 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";

// This file essentially reproduces an example Angular component to map testing,
// among other typescript edge cases.
var __extends = (this && this.__extends) || (function () {
    var extendStatics = Object.setPrototypeOf ||
        ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
        function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
exports.__esModule = true;
var mod_ts_1 = __webpack_require__(0);
var ns = __webpack_require__(0);
var AppComponent = /** @class */ (function () {
    function AppComponent() {
        this.title = 'app';
    }
    AppComponent = __decorate([
        mod_ts_1.decoratorFactory({
            selector: 'app-root'
        })
    ], AppComponent);
    return AppComponent;
}());
exports.AppComponent = AppComponent;
var fn = function (arg) {
    console.log("here");
};
fn("arg");
// Un-decorated exported classes present a mapping challege because
// the class name is mapped to an unhelpful export assignment.
var ExportedOther = /** @class */ (function () {
    function ExportedOther() {
        this.title = 'app';
    }
    return ExportedOther;
}());
exports.ExportedOther = ExportedOther;
var AnotherThing = /** @class */ (function () {
    function AnotherThing() {
        this.prop = 4;
    }
    return AnotherThing;
}());
var ExpressionClass = /** @class */ (function () {
    function Foo() {
        this.prop = 4;
    }
    return Foo;
}());
var SubDecl = /** @class */ (function (_super) {
    __extends(SubDecl, _super);
    function SubDecl() {
        var _this = _super !== null && _super.apply(this, arguments) || this;
        _this.prop = 4;
        return _this;
    }
    return SubDecl;
}(AnotherThing));
var SubVar = /** @class */ (function (_super) {
    __extends(SubExpr, _super);
    function SubExpr() {
        var _this = _super !== null && _super.apply(this, arguments) || this;
        _this.prop = 4;
        return _this;
    }
    return SubExpr;
}(AnotherThing));
ns;
function default_1() {
    // This file is specifically for testing the mappings of classes and things
    // above, which means we don't want to include _other_ references to then.
    // To avoid having them be optimized out, we include a no-op eval.
    eval("");
    console.log("pause here");
}
exports["default"] = default_1;


/***/ })
/******/ ])["default"];
//# sourceMappingURL=typescript-classes.js.map