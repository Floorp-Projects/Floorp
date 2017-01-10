/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
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
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

	const times2 = __webpack_require__(1);
	const { output } = __webpack_require__(2);
	const opts = __webpack_require__(3);
	
	output(times2(1));
	output(times2(2));
	
	if(opts.extra) {
	  output(times2(3));
	}
	
	window.keepMeAlive = function() {
	  // This function exists to make sure this script is never garbage
	  // collected. It is also callable from tests.
	  return times2(4);
	}


/***/ },
/* 1 */
/***/ function(module, exports) {

	module.exports = function(x) {
	  return x * 2;
	}


/***/ },
/* 2 */
/***/ function(module, exports) {

	function output(str) {
	  console.log(str);
	}
	
	module.exports = { output };


/***/ },
/* 3 */
/***/ function(module, exports) {

	module.exports = {
	  extra: true
	};


/***/ }
/******/ ]);
//# sourceMappingURL=bundle.js.map