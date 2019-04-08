var webpack4EsmodulesEs6 =
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
/*! exports provided: default, example */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "default", function() { return root; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "example", function() { return example; });
/* harmony import */ var _src_mod1__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./src/mod1 */ "./src/mod1.js");
/* harmony import */ var _src_mod2__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./src/mod2 */ "./src/mod2.js");
/* harmony import */ var _src_mod3__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./src/mod3 */ "./src/mod3.js");
/* harmony import */ var _src_mod4__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./src/mod4 */ "./src/mod4.js");
/* harmony import */ var _src_mod5__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./src/mod5 */ "./src/mod5.js");
/* harmony import */ var _src_mod6__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./src/mod6 */ "./src/mod6.js");
/* harmony import */ var _src_mod7__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./src/mod7 */ "./src/mod7.js");
/* harmony import */ var _src_mod9__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./src/mod9 */ "./src/mod9.js");
/* harmony import */ var _src_mod10__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! ./src/mod10 */ "./src/mod10.js");
/* harmony import */ var _src_mod11__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(/*! ./src/mod11 */ "./src/mod11.js");
/* harmony import */ var _src_optimized_out__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(/*! ./src/optimized-out */ "./src/optimized-out.js");














Object(_src_optimized_out__WEBPACK_IMPORTED_MODULE_10__["default"])();

function root() {
  console.log("pause here", root);

  console.log(_src_mod1__WEBPACK_IMPORTED_MODULE_0__["default"]);
  console.log(_src_mod3__WEBPACK_IMPORTED_MODULE_2__["original"]);
  console.log(_src_mod2__WEBPACK_IMPORTED_MODULE_1__["aNamed"]);
  console.log(_src_mod2__WEBPACK_IMPORTED_MODULE_1__["aNamed"]);
  console.log(_src_mod4__WEBPACK_IMPORTED_MODULE_3__);

  try {
    // None of these are callable in this code, but we still want to make sure
    // they map properly even if the only reference is in a call expressions.
    console.log(Object(_src_mod5__WEBPACK_IMPORTED_MODULE_4__["default"])());
    console.log(Object(_src_mod7__WEBPACK_IMPORTED_MODULE_6__["original"])());
    console.log(Object(_src_mod6__WEBPACK_IMPORTED_MODULE_5__["aNamed2"])());
    console.log(Object(_src_mod6__WEBPACK_IMPORTED_MODULE_5__["aNamed2"])());

    console.log(new _src_mod9__WEBPACK_IMPORTED_MODULE_7__["default"]());
    console.log(new _src_mod11__WEBPACK_IMPORTED_MODULE_9__["original"]());
    console.log(new _src_mod10__WEBPACK_IMPORTED_MODULE_8__["aNamed3"]());
    console.log(new _src_mod10__WEBPACK_IMPORTED_MODULE_8__["aNamed3"]());
  } catch (e) {}
}

function example(){}


/***/ }),

/***/ "./src/mod1.js":
/*!*********************!*\
  !*** ./src/mod1.js ***!
  \*********************/
/*! exports provided: default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony default export */ __webpack_exports__["default"] = ("a-default");


/***/ }),

/***/ "./src/mod10.js":
/*!**********************!*\
  !*** ./src/mod10.js ***!
  \**********************/
/*! exports provided: aNamed3 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "aNamed3", function() { return aNamed3; });
const aNamed3 = "a-named3";


/***/ }),

/***/ "./src/mod11.js":
/*!**********************!*\
  !*** ./src/mod11.js ***!
  \**********************/
/*! exports provided: original */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "original", function() { return original; });
const original = "an-original3";


/***/ }),

/***/ "./src/mod2.js":
/*!*********************!*\
  !*** ./src/mod2.js ***!
  \*********************/
/*! exports provided: aNamed */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "aNamed", function() { return aNamed; });
const aNamed = "a-named";


/***/ }),

/***/ "./src/mod3.js":
/*!*********************!*\
  !*** ./src/mod3.js ***!
  \*********************/
/*! exports provided: original */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "original", function() { return original; });
const original = "an-original";


/***/ }),

/***/ "./src/mod4.js":
/*!*********************!*\
  !*** ./src/mod4.js ***!
  \*********************/
/*! exports provided: default, aNamed */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "aNamed", function() { return aNamed; });
/* harmony default export */ __webpack_exports__["default"] = ("a-default");
const aNamed = "a-named";


/***/ }),

/***/ "./src/mod5.js":
/*!*********************!*\
  !*** ./src/mod5.js ***!
  \*********************/
/*! exports provided: default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony default export */ __webpack_exports__["default"] = ("a-default2");


/***/ }),

/***/ "./src/mod6.js":
/*!*********************!*\
  !*** ./src/mod6.js ***!
  \*********************/
/*! exports provided: aNamed2 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "aNamed2", function() { return aNamed2; });
const aNamed2 = "a-named2";


/***/ }),

/***/ "./src/mod7.js":
/*!*********************!*\
  !*** ./src/mod7.js ***!
  \*********************/
/*! exports provided: original */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "original", function() { return original; });
const original = "an-original2";


/***/ }),

/***/ "./src/mod9.js":
/*!*********************!*\
  !*** ./src/mod9.js ***!
  \*********************/
/*! exports provided: default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony default export */ __webpack_exports__["default"] = ("a-default3");


/***/ }),

/***/ "./src/optimized-out.js":
/*!******************************!*\
  !*** ./src/optimized-out.js ***!
  \******************************/
/*! exports provided: default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "default", function() { return optimizedOut; });
function optimizedOut() {}


/***/ })

/******/ })["default"];
//# sourceMappingURL=esmodules-es6.js.map