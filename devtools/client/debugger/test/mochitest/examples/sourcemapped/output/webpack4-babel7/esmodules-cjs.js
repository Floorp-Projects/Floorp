var webpack4Babel7EsmodulesCjs =
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
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
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
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = "./input.js");
/******/ })
/************************************************************************/
/******/ ({

/***/ "./input.js":
/*!******************!*\
  !*** ./input.js ***!
  \******************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = root;
exports.example = example;

var _mod = _interopRequireDefault(__webpack_require__(/*! ./src/mod1 */ "./src/mod1.js"));

var _mod2 = __webpack_require__(/*! ./src/mod2 */ "./src/mod2.js");

var _mod3 = __webpack_require__(/*! ./src/mod3 */ "./src/mod3.js");

var aNamespace = _interopRequireWildcard(__webpack_require__(/*! ./src/mod4 */ "./src/mod4.js"));

var _mod5 = _interopRequireDefault(__webpack_require__(/*! ./src/mod5 */ "./src/mod5.js"));

var _mod6 = __webpack_require__(/*! ./src/mod6 */ "./src/mod6.js");

var _mod7 = __webpack_require__(/*! ./src/mod7 */ "./src/mod7.js");

var _mod8 = _interopRequireDefault(__webpack_require__(/*! ./src/mod9 */ "./src/mod9.js"));

var _mod9 = __webpack_require__(/*! ./src/mod10 */ "./src/mod10.js");

var _mod10 = __webpack_require__(/*! ./src/mod11 */ "./src/mod11.js");

var _optimizedOut = _interopRequireDefault(__webpack_require__(/*! ./src/optimized-out */ "./src/optimized-out.js"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

(0, _optimizedOut.default)();

function root() {
  console.log("pause here", root);
  console.log(_mod.default);
  console.log(_mod3.original);
  console.log(_mod2.aNamed);
  console.log(_mod2.aNamed);
  console.log(aNamespace);

  try {
    // None of these are callable in this code, but we still want to make sure
    // they map properly even if the only reference is in a call expressions.
    console.log((0, _mod5.default)());
    console.log((0, _mod7.original)());
    console.log((0, _mod6.aNamed2)());
    console.log((0, _mod6.aNamed2)());
    console.log(new _mod8.default());
    console.log(new _mod10.original());
    console.log(new _mod9.aNamed3());
    console.log(new _mod9.aNamed3());
  } catch (e) {}
}

function example() {}

/***/ }),

/***/ "./src/mod1.js":
/*!*********************!*\
  !*** ./src/mod1.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _default = "a-default";
exports.default = _default;

/***/ }),

/***/ "./src/mod10.js":
/*!**********************!*\
  !*** ./src/mod10.js ***!
  \**********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.aNamed3 = void 0;
var aNamed3 = "a-named3";
exports.aNamed3 = aNamed3;

/***/ }),

/***/ "./src/mod11.js":
/*!**********************!*\
  !*** ./src/mod11.js ***!
  \**********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.original = void 0;
var original = "an-original3";
exports.original = original;

/***/ }),

/***/ "./src/mod2.js":
/*!*********************!*\
  !*** ./src/mod2.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.aNamed = void 0;
var aNamed = "a-named";
exports.aNamed = aNamed;

/***/ }),

/***/ "./src/mod3.js":
/*!*********************!*\
  !*** ./src/mod3.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.original = void 0;
var original = "an-original";
exports.original = original;

/***/ }),

/***/ "./src/mod4.js":
/*!*********************!*\
  !*** ./src/mod4.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.aNamed = exports.default = void 0;
var _default = "a-default";
exports.default = _default;
var aNamed = "a-named";
exports.aNamed = aNamed;

/***/ }),

/***/ "./src/mod5.js":
/*!*********************!*\
  !*** ./src/mod5.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _default = "a-default2";
exports.default = _default;

/***/ }),

/***/ "./src/mod6.js":
/*!*********************!*\
  !*** ./src/mod6.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.aNamed2 = void 0;
var aNamed2 = "a-named2";
exports.aNamed2 = aNamed2;

/***/ }),

/***/ "./src/mod7.js":
/*!*********************!*\
  !*** ./src/mod7.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.original = void 0;
var original = "an-original2";
exports.original = original;

/***/ }),

/***/ "./src/mod9.js":
/*!*********************!*\
  !*** ./src/mod9.js ***!
  \*********************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _default = "a-default3";
exports.default = _default;

/***/ }),

/***/ "./src/optimized-out.js":
/*!******************************!*\
  !*** ./src/optimized-out.js ***!
  \******************************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = optimizedOut;

function optimizedOut() {}

/***/ })

/******/ })["default"];
//# sourceMappingURL=esmodules-cjs.js.map