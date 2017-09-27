function run_test() {
  for (var k in SOURCE_MAP_TEST_MODULE) {
    if (/^test/.test(k)) {
      SOURCE_MAP_TEST_MODULE[k](assert);
    }
  }
}


var SOURCE_MAP_TEST_MODULE =
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
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(1);
	var SourceMapConsumer = __webpack_require__(3).SourceMapConsumer;
	var IndexedSourceMapConsumer = __webpack_require__(3).IndexedSourceMapConsumer;
	var BasicSourceMapConsumer = __webpack_require__(3).BasicSourceMapConsumer;
	var SourceMapGenerator = __webpack_require__(9).SourceMapGenerator;
	
	exports['test that we can instantiate with a string or an object'] = function (assert) {
	  assert.doesNotThrow(function () {
	    var map = new SourceMapConsumer(util.testMap);
	  });
	  assert.doesNotThrow(function () {
	    var map = new SourceMapConsumer(JSON.stringify(util.testMap));
	  });
	};
	
	exports['test that the object returned from new SourceMapConsumer inherits from SourceMapConsumer'] = function (assert) {
	  assert.ok(new SourceMapConsumer(util.testMap) instanceof SourceMapConsumer);
	}
	
	exports['test that a BasicSourceMapConsumer is returned for sourcemaps without sections'] = function(assert) {
	  assert.ok(new SourceMapConsumer(util.testMap) instanceof BasicSourceMapConsumer);
	};
	
	exports['test that an IndexedSourceMapConsumer is returned for sourcemaps with sections'] = function(assert) {
	  assert.ok(new SourceMapConsumer(util.indexedTestMap) instanceof IndexedSourceMapConsumer);
	};
	
	exports['test that the `sources` field has the original sources'] = function (assert) {
	  var map;
	  var sources;
	
	  map = new SourceMapConsumer(util.testMap);
	  sources = map.sources;
	  assert.equal(sources[0], '/the/root/one.js');
	  assert.equal(sources[1], '/the/root/two.js');
	  assert.equal(sources.length, 2);
	
	  map = new SourceMapConsumer(util.indexedTestMap);
	  sources = map.sources;
	  assert.equal(sources[0], '/the/root/one.js');
	  assert.equal(sources[1], '/the/root/two.js');
	  assert.equal(sources.length, 2);
	
	  map = new SourceMapConsumer(util.indexedTestMapDifferentSourceRoots);
	  sources = map.sources;
	  assert.equal(sources[0], '/the/root/one.js');
	  assert.equal(sources[1], '/different/root/two.js');
	  assert.equal(sources.length, 2);
	
	  map = new SourceMapConsumer(util.testMapNoSourceRoot);
	  sources = map.sources;
	  assert.equal(sources[0], 'one.js');
	  assert.equal(sources[1], 'two.js');
	  assert.equal(sources.length, 2);
	
	  map = new SourceMapConsumer(util.testMapEmptySourceRoot);
	  sources = map.sources;
	  assert.equal(sources[0], 'one.js');
	  assert.equal(sources[1], 'two.js');
	  assert.equal(sources.length, 2);
	};
	
	exports['test that the source root is reflected in a mapping\'s source field'] = function (assert) {
	  var map;
	  var mapping;
	
	  map = new SourceMapConsumer(util.testMap);
	
	  mapping = map.originalPositionFor({
	    line: 2,
	    column: 1
	  });
	  assert.equal(mapping.source, '/the/root/two.js');
	
	  mapping = map.originalPositionFor({
	    line: 1,
	    column: 1
	  });
	  assert.equal(mapping.source, '/the/root/one.js');
	
	
	  map = new SourceMapConsumer(util.testMapNoSourceRoot);
	
	  mapping = map.originalPositionFor({
	    line: 2,
	    column: 1
	  });
	  assert.equal(mapping.source, 'two.js');
	
	  mapping = map.originalPositionFor({
	    line: 1,
	    column: 1
	  });
	  assert.equal(mapping.source, 'one.js');
	
	
	  map = new SourceMapConsumer(util.testMapEmptySourceRoot);
	
	  mapping = map.originalPositionFor({
	    line: 2,
	    column: 1
	  });
	  assert.equal(mapping.source, 'two.js');
	
	  mapping = map.originalPositionFor({
	    line: 1,
	    column: 1
	  });
	  assert.equal(mapping.source, 'one.js');
	};
	
	exports['test mapping tokens back exactly'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMap);
	
	  util.assertMapping(1, 1, '/the/root/one.js', 1, 1, null, null, map, assert);
	  util.assertMapping(1, 5, '/the/root/one.js', 1, 5, null, null, map, assert);
	  util.assertMapping(1, 9, '/the/root/one.js', 1, 11, null, null, map, assert);
	  util.assertMapping(1, 18, '/the/root/one.js', 1, 21, 'bar', null, map, assert);
	  util.assertMapping(1, 21, '/the/root/one.js', 2, 3, null, null, map, assert);
	  util.assertMapping(1, 28, '/the/root/one.js', 2, 10, 'baz', null, map, assert);
	  util.assertMapping(1, 32, '/the/root/one.js', 2, 14, 'bar', null, map, assert);
	
	  util.assertMapping(2, 1, '/the/root/two.js', 1, 1, null, null, map, assert);
	  util.assertMapping(2, 5, '/the/root/two.js', 1, 5, null, null, map, assert);
	  util.assertMapping(2, 9, '/the/root/two.js', 1, 11, null, null, map, assert);
	  util.assertMapping(2, 18, '/the/root/two.js', 1, 21, 'n', null, map, assert);
	  util.assertMapping(2, 21, '/the/root/two.js', 2, 3, null, null, map, assert);
	  util.assertMapping(2, 28, '/the/root/two.js', 2, 10, 'n', null, map, assert);
	};
	
	exports['test mapping tokens back exactly in indexed source map'] = function (assert) {
	  var map = new SourceMapConsumer(util.indexedTestMap);
	
	  util.assertMapping(1, 1, '/the/root/one.js', 1, 1, null, null, map, assert);
	  util.assertMapping(1, 5, '/the/root/one.js', 1, 5, null, null, map, assert);
	  util.assertMapping(1, 9, '/the/root/one.js', 1, 11, null, null, map, assert);
	  util.assertMapping(1, 18, '/the/root/one.js', 1, 21, 'bar', null, map, assert);
	  util.assertMapping(1, 21, '/the/root/one.js', 2, 3, null, null, map, assert);
	  util.assertMapping(1, 28, '/the/root/one.js', 2, 10, 'baz', null, map, assert);
	  util.assertMapping(1, 32, '/the/root/one.js', 2, 14, 'bar', null, map, assert);
	
	  util.assertMapping(2, 1, '/the/root/two.js', 1, 1, null, null, map, assert);
	  util.assertMapping(2, 5, '/the/root/two.js', 1, 5, null, null, map, assert);
	  util.assertMapping(2, 9, '/the/root/two.js', 1, 11, null, null, map, assert);
	  util.assertMapping(2, 18, '/the/root/two.js', 1, 21, 'n', null, map, assert);
	  util.assertMapping(2, 21, '/the/root/two.js', 2, 3, null, null, map, assert);
	  util.assertMapping(2, 28, '/the/root/two.js', 2, 10, 'n', null, map, assert);
	};
	
	exports['test mapping tokens fuzzy'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMap);
	
	  // Finding original positions with default (glb) bias.
	  util.assertMapping(1, 20, '/the/root/one.js', 1, 21, 'bar', null, map, assert, true);
	  util.assertMapping(1, 30, '/the/root/one.js', 2, 10, 'baz', null, map, assert, true);
	  util.assertMapping(2, 12, '/the/root/two.js', 1, 11, null, null, map, assert, true);
	
	  // Finding original positions with lub bias.
	  util.assertMapping(1, 16, '/the/root/one.js', 1, 21, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	  util.assertMapping(1, 26, '/the/root/one.js', 2, 10, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	  util.assertMapping(2, 6, '/the/root/two.js', 1, 11, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	
	  // Finding generated positions with default (glb) bias.
	  util.assertMapping(1, 18, '/the/root/one.js', 1, 22, 'bar', null, map, assert, null, true);
	  util.assertMapping(1, 28, '/the/root/one.js', 2, 13, 'baz', null, map, assert, null, true);
	  util.assertMapping(2, 9, '/the/root/two.js', 1, 16, null, null, map, assert, null, true);
	
	  // Finding generated positions with lub bias.
	  util.assertMapping(1, 18, '/the/root/one.js', 1, 20, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  util.assertMapping(1, 28, '/the/root/one.js', 2, 7, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  util.assertMapping(2, 9, '/the/root/two.js', 1, 6, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	};
	
	exports['test mapping tokens fuzzy in indexed source map'] = function (assert) {
	  var map = new SourceMapConsumer(util.indexedTestMap);
	
	  // Finding original positions with default (glb) bias.
	  util.assertMapping(1, 20, '/the/root/one.js', 1, 21, 'bar', null, map, assert, true);
	  util.assertMapping(1, 30, '/the/root/one.js', 2, 10, 'baz', null, map, assert, true);
	  util.assertMapping(2, 12, '/the/root/two.js', 1, 11, null, null, map, assert, true);
	
	  // Finding original positions with lub bias.
	  util.assertMapping(1, 16, '/the/root/one.js', 1, 21, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	  util.assertMapping(1, 26, '/the/root/one.js', 2, 10, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	  util.assertMapping(2, 6, '/the/root/two.js', 1, 11, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	
	  // Finding generated positions with default (glb) bias.
	  util.assertMapping(1, 18, '/the/root/one.js', 1, 22, 'bar', null, map, assert, null, true);
	  util.assertMapping(1, 28, '/the/root/one.js', 2, 13, 'baz', null, map, assert, null, true);
	  util.assertMapping(2, 9, '/the/root/two.js', 1, 16, null, null, map, assert, null, true);
	
	  // Finding generated positions with lub bias.
	  util.assertMapping(1, 18, '/the/root/one.js', 1, 20, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  util.assertMapping(1, 28, '/the/root/one.js', 2, 7, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  util.assertMapping(2, 9, '/the/root/two.js', 1, 6, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	};
	
	exports['test mappings and end of lines'] = function (assert) {
	  var smg = new SourceMapGenerator({
	    file: 'foo.js'
	  });
	  smg.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 1 },
	    source: 'bar.js'
	  });
	  smg.addMapping({
	    original: { line: 2, column: 2 },
	    generated: { line: 2, column: 2 },
	    source: 'bar.js'
	  });
	  smg.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 1 },
	    source: 'baz.js'
	  });
	
	  var map = SourceMapConsumer.fromSourceMap(smg);
	
	  // When finding original positions, mappings end at the end of the line.
	  util.assertMapping(2, 1, null, null, null, null, null, map, assert, true)
	
	  // When finding generated positions, mappings do not end at the end of the line.
	  util.assertMapping(1, 1, 'bar.js', 2, 1, null, null, map, assert, null, true);
	
	  // When finding generated positions with, mappings end at the end of the source.
	  util.assertMapping(null, null, 'bar.js', 3, 1, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	};
	
	exports['test creating source map consumers with )]}\' prefix'] = function (assert) {
	  assert.doesNotThrow(function () {
	    var map = new SourceMapConsumer(")]}'\n" + JSON.stringify(util.testMap));
	  });
	};
	
	exports['test eachMapping'] = function (assert) {
	  var map;
	
	  map = new SourceMapConsumer(util.testMap);
	  var previousLine = -Infinity;
	  var previousColumn = -Infinity;
	  map.eachMapping(function (mapping) {
	    assert.ok(mapping.generatedLine >= previousLine);
	
	    assert.ok(mapping.source === '/the/root/one.js' || mapping.source === '/the/root/two.js');
	
	    if (mapping.generatedLine === previousLine) {
	      assert.ok(mapping.generatedColumn >= previousColumn);
	      previousColumn = mapping.generatedColumn;
	    }
	    else {
	      previousLine = mapping.generatedLine;
	      previousColumn = -Infinity;
	    }
	  });
	
	  map = new SourceMapConsumer(util.testMapNoSourceRoot);
	  map.eachMapping(function (mapping) {
	    assert.ok(mapping.source === 'one.js' || mapping.source === 'two.js');
	  });
	
	  map = new SourceMapConsumer(util.testMapEmptySourceRoot);
	  map.eachMapping(function (mapping) {
	    assert.ok(mapping.source === 'one.js' || mapping.source === 'two.js');
	  });
	};
	
	exports['test eachMapping for indexed source maps'] = function(assert) {
	  var map = new SourceMapConsumer(util.indexedTestMap);
	  var previousLine = -Infinity;
	  var previousColumn = -Infinity;
	  map.eachMapping(function (mapping) {
	    assert.ok(mapping.generatedLine >= previousLine);
	
	    if (mapping.source) {
	      assert.equal(mapping.source.indexOf(util.testMap.sourceRoot), 0);
	    }
	
	    if (mapping.generatedLine === previousLine) {
	      assert.ok(mapping.generatedColumn >= previousColumn);
	      previousColumn = mapping.generatedColumn;
	    }
	    else {
	      previousLine = mapping.generatedLine;
	      previousColumn = -Infinity;
	    }
	  });
	};
	
	
	exports['test iterating over mappings in a different order'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMap);
	  var previousLine = -Infinity;
	  var previousColumn = -Infinity;
	  var previousSource = "";
	  map.eachMapping(function (mapping) {
	    assert.ok(mapping.source >= previousSource);
	
	    if (mapping.source === previousSource) {
	      assert.ok(mapping.originalLine >= previousLine);
	
	      if (mapping.originalLine === previousLine) {
	        assert.ok(mapping.originalColumn >= previousColumn);
	        previousColumn = mapping.originalColumn;
	      }
	      else {
	        previousLine = mapping.originalLine;
	        previousColumn = -Infinity;
	      }
	    }
	    else {
	      previousSource = mapping.source;
	      previousLine = -Infinity;
	      previousColumn = -Infinity;
	    }
	  }, null, SourceMapConsumer.ORIGINAL_ORDER);
	};
	
	exports['test iterating over mappings in a different order in indexed source maps'] = function (assert) {
	  var map = new SourceMapConsumer(util.indexedTestMap);
	  var previousLine = -Infinity;
	  var previousColumn = -Infinity;
	  var previousSource = "";
	  map.eachMapping(function (mapping) {
	    assert.ok(mapping.source >= previousSource);
	
	    if (mapping.source === previousSource) {
	      assert.ok(mapping.originalLine >= previousLine);
	
	      if (mapping.originalLine === previousLine) {
	        assert.ok(mapping.originalColumn >= previousColumn);
	        previousColumn = mapping.originalColumn;
	      }
	      else {
	        previousLine = mapping.originalLine;
	        previousColumn = -Infinity;
	      }
	    }
	    else {
	      previousSource = mapping.source;
	      previousLine = -Infinity;
	      previousColumn = -Infinity;
	    }
	  }, null, SourceMapConsumer.ORIGINAL_ORDER);
	};
	
	exports['test that we can set the context for `this` in eachMapping'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMap);
	  var context = {};
	  map.eachMapping(function () {
	    assert.equal(this, context);
	  }, context);
	};
	
	exports['test that we can set the context for `this` in eachMapping in indexed source maps'] = function (assert) {
	  var map = new SourceMapConsumer(util.indexedTestMap);
	  var context = {};
	  map.eachMapping(function () {
	    assert.equal(this, context);
	  }, context);
	};
	
	exports['test that the `sourcesContent` field has the original sources'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMapWithSourcesContent);
	  var sourcesContent = map.sourcesContent;
	
	  assert.equal(sourcesContent[0], ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(sourcesContent[1], ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.equal(sourcesContent.length, 2);
	};
	
	exports['test that we can get the original sources for the sources'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMapWithSourcesContent);
	  var sources = map.sources;
	
	  assert.equal(map.sourceContentFor(sources[0]), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(map.sourceContentFor(sources[1]), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.equal(map.sourceContentFor("one.js"), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(map.sourceContentFor("two.js"), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.throws(function () {
	    map.sourceContentFor("");
	  }, Error);
	  assert.throws(function () {
	    map.sourceContentFor("/the/root/three.js");
	  }, Error);
	  assert.throws(function () {
	    map.sourceContentFor("three.js");
	  }, Error);
	};
	
	exports['test that we can get the original source content with relative source paths'] = function (assert) {
	  var map = new SourceMapConsumer(util.testMapRelativeSources);
	  var sources = map.sources;
	
	  assert.equal(map.sourceContentFor(sources[0]), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(map.sourceContentFor(sources[1]), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.equal(map.sourceContentFor("one.js"), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(map.sourceContentFor("two.js"), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.throws(function () {
	    map.sourceContentFor("");
	  }, Error);
	  assert.throws(function () {
	    map.sourceContentFor("/the/root/three.js");
	  }, Error);
	  assert.throws(function () {
	    map.sourceContentFor("three.js");
	  }, Error);
	};
	
	exports['test that we can get the original source content for the sources on an indexed source map'] = function (assert) {
	  var map = new SourceMapConsumer(util.indexedTestMap);
	  var sources = map.sources;
	
	  assert.equal(map.sourceContentFor(sources[0]), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(map.sourceContentFor(sources[1]), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.equal(map.sourceContentFor("one.js"), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	  assert.equal(map.sourceContentFor("two.js"), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	  assert.throws(function () {
	    map.sourceContentFor("");
	  }, Error);
	  assert.throws(function () {
	    map.sourceContentFor("/the/root/three.js");
	  }, Error);
	  assert.throws(function () {
	    map.sourceContentFor("three.js");
	  }, Error);
	};
	
	exports['test hasContentsOfAllSources, single source with contents'] = function (assert) {
	  // Has one source: foo.js (with contents).
	  var mapWithContents = new SourceMapGenerator();
	  mapWithContents.addMapping({ source: 'foo.js',
	                               original: { line: 1, column: 10 },
	                               generated: { line: 1, column: 10 } });
	  mapWithContents.setSourceContent('foo.js', 'content of foo.js');
	  var consumer = new SourceMapConsumer(mapWithContents.toJSON());
	  assert.ok(consumer.hasContentsOfAllSources());
	};
	
	exports['test hasContentsOfAllSources, single source without contents'] = function (assert) {
	  // Has one source: foo.js (without contents).
	  var mapWithoutContents = new SourceMapGenerator();
	  mapWithoutContents.addMapping({ source: 'foo.js',
	                                  original: { line: 1, column: 10 },
	                                  generated: { line: 1, column: 10 } });
	  var consumer = new SourceMapConsumer(mapWithoutContents.toJSON());
	  assert.ok(!consumer.hasContentsOfAllSources());
	};
	
	exports['test hasContentsOfAllSources, two sources with contents'] = function (assert) {
	  // Has two sources: foo.js (with contents) and bar.js (with contents).
	  var mapWithBothContents = new SourceMapGenerator();
	  mapWithBothContents.addMapping({ source: 'foo.js',
	                                   original: { line: 1, column: 10 },
	                                   generated: { line: 1, column: 10 } });
	  mapWithBothContents.addMapping({ source: 'bar.js',
	                                   original: { line: 1, column: 10 },
	                                   generated: { line: 1, column: 10 } });
	  mapWithBothContents.setSourceContent('foo.js', 'content of foo.js');
	  mapWithBothContents.setSourceContent('bar.js', 'content of bar.js');
	  var consumer = new SourceMapConsumer(mapWithBothContents.toJSON());
	  assert.ok(consumer.hasContentsOfAllSources());
	};
	
	exports['test hasContentsOfAllSources, two sources one with and one without contents'] = function (assert) {
	  // Has two sources: foo.js (with contents) and bar.js (without contents).
	  var mapWithoutSomeContents = new SourceMapGenerator();
	  mapWithoutSomeContents.addMapping({ source: 'foo.js',
	                                      original: { line: 1, column: 10 },
	                                      generated: { line: 1, column: 10 } });
	  mapWithoutSomeContents.addMapping({ source: 'bar.js',
	                                      original: { line: 1, column: 10 },
	                                      generated: { line: 1, column: 10 } });
	  mapWithoutSomeContents.setSourceContent('foo.js', 'content of foo.js');
	  var consumer = new SourceMapConsumer(mapWithoutSomeContents.toJSON());
	  assert.ok(!consumer.hasContentsOfAllSources());
	};
	
	exports['test sourceRoot + generatedPositionFor'] = function (assert) {
	  var map = new SourceMapGenerator({
	    sourceRoot: 'foo/bar',
	    file: 'baz.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'bang.coffee'
	  });
	  map.addMapping({
	    original: { line: 5, column: 5 },
	    generated: { line: 6, column: 6 },
	    source: 'bang.coffee'
	  });
	  map = new SourceMapConsumer(map.toString(), 'http://example.com/');
	
	  // Should handle without sourceRoot.
	  var pos = map.generatedPositionFor({
	    line: 1,
	    column: 1,
	    source: 'bang.coffee'
	  });
	
	  assert.equal(pos.line, 2);
	  assert.equal(pos.column, 2);
	
	  // Should handle with sourceRoot.
	  var pos = map.generatedPositionFor({
	    line: 1,
	    column: 1,
	    source: 'foo/bar/bang.coffee'
	  });
	
	  assert.equal(pos.line, 2);
	  assert.equal(pos.column, 2);
	
	  // Should handle absolute case.
	  var pos = map.generatedPositionFor({
	    line: 1,
	    column: 1,
	    source: 'http://example.com/foo/bar/bang.coffee'
	  });
	
	  assert.equal(pos.line, 2);
	  assert.equal(pos.column, 2);
	};
	
	exports['test sourceRoot + generatedPositionFor for path above the root'] = function (assert) {
	  var map = new SourceMapGenerator({
	    sourceRoot: 'foo/bar',
	    file: 'baz.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: '../bang.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  // Should handle with sourceRoot.
	  var pos = map.generatedPositionFor({
	    line: 1,
	    column: 1,
	    source: 'foo/bang.coffee'
	  });
	
	  assert.equal(pos.line, 2);
	  assert.equal(pos.column, 2);
	};
	
	exports['test allGeneratedPositionsFor for line'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'bar.coffee'
	  });
	  map.addMapping({
	    original: { line: 2, column: 1 },
	    generated: { line: 3, column: 2 },
	    source: 'bar.coffee'
	  });
	  map.addMapping({
	    original: { line: 2, column: 2 },
	    generated: { line: 3, column: 3 },
	    source: 'bar.coffee'
	  });
	  map.addMapping({
	    original: { line: 3, column: 1 },
	    generated: { line: 4, column: 2 },
	    source: 'bar.coffee'
	  });
	  map = new SourceMapConsumer(map.toString(), 'http://example.com/');
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 2,
	    source: 'bar.coffee'
	  });
	
	  assert.equal(mappings.length, 2);
	  assert.equal(mappings[0].line, 3);
	  assert.equal(mappings[0].column, 2);
	  assert.equal(mappings[1].line, 3);
	  assert.equal(mappings[1].column, 3);
	
	  mappings = map.allGeneratedPositionsFor({
	    line: 2,
	    source: 'http://example.com/bar.coffee'
	  });
	
	  assert.equal(mappings.length, 2);
	  assert.equal(mappings[0].line, 3);
	  assert.equal(mappings[0].column, 2);
	  assert.equal(mappings[1].line, 3);
	  assert.equal(mappings[1].column, 3);
	};
	
	exports['test allGeneratedPositionsFor for line fuzzy'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'bar.coffee'
	  });
	  map.addMapping({
	    original: { line: 3, column: 1 },
	    generated: { line: 4, column: 2 },
	    source: 'bar.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 2,
	    source: 'bar.coffee'
	  });
	
	  assert.equal(mappings.length, 1);
	  assert.equal(mappings[0].line, 4);
	  assert.equal(mappings[0].column, 2);
	};
	
	exports['test allGeneratedPositionsFor for empty source map'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 2,
	    source: 'bar.coffee'
	  });
	
	  assert.equal(mappings.length, 0);
	};
	
	exports['test allGeneratedPositionsFor for column'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 2 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 3 },
	    source: 'foo.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 1,
	    column: 1,
	    source: 'foo.coffee'
	  });
	
	  assert.equal(mappings.length, 2);
	  assert.equal(mappings[0].line, 1);
	  assert.equal(mappings[0].column, 2);
	  assert.equal(mappings[1].line, 1);
	  assert.equal(mappings[1].column, 3);
	};
	
	exports['test allGeneratedPositionsFor for column fuzzy'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 2 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 3 },
	    source: 'foo.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 1,
	    column: 0,
	    source: 'foo.coffee'
	  });
	
	  assert.equal(mappings.length, 2);
	  assert.equal(mappings[0].line, 1);
	  assert.equal(mappings[0].column, 2);
	  assert.equal(mappings[1].line, 1);
	  assert.equal(mappings[1].column, 3);
	};
	
	exports['test allGeneratedPositionsFor for column on different line fuzzy'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map.addMapping({
	    original: { line: 2, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 2, column: 1 },
	    generated: { line: 2, column: 3 },
	    source: 'foo.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 1,
	    column: 0,
	    source: 'foo.coffee'
	  });
	
	  assert.equal(mappings.length, 0);
	};
	
	exports['test computeColumnSpans'] = function (assert) {
	  var map = new SourceMapGenerator({
	    file: 'generated.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 1, column: 1 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 2, column: 1 },
	    generated: { line: 2, column: 1 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 2, column: 2 },
	    generated: { line: 2, column: 10 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 2, column: 3 },
	    generated: { line: 2, column: 20 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 3, column: 1 },
	    generated: { line: 3, column: 1 },
	    source: 'foo.coffee'
	  });
	  map.addMapping({
	    original: { line: 3, column: 2 },
	    generated: { line: 3, column: 2 },
	    source: 'foo.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  map.computeColumnSpans();
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 1,
	    source: 'foo.coffee'
	  });
	
	  assert.equal(mappings.length, 1);
	  assert.equal(mappings[0].lastColumn, Infinity);
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 2,
	    source: 'foo.coffee'
	  });
	
	  assert.equal(mappings.length, 3);
	  assert.equal(mappings[0].lastColumn, 9);
	  assert.equal(mappings[1].lastColumn, 19);
	  assert.equal(mappings[2].lastColumn, Infinity);
	
	  var mappings = map.allGeneratedPositionsFor({
	    line: 3,
	    source: 'foo.coffee'
	  });
	
	  assert.equal(mappings.length, 2);
	  assert.equal(mappings[0].lastColumn, 1);
	  assert.equal(mappings[1].lastColumn, Infinity);
	};
	
	exports['test sourceRoot + originalPositionFor'] = function (assert) {
	  var map = new SourceMapGenerator({
	    sourceRoot: 'foo/bar',
	    file: 'baz.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'bang.coffee'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var pos = map.originalPositionFor({
	    line: 2,
	    column: 2,
	  });
	
	  // Should always have the prepended source root
	  assert.equal(pos.source, 'foo/bar/bang.coffee');
	  assert.equal(pos.line, 1);
	  assert.equal(pos.column, 1);
	};
	
	exports['test github issue #56'] = function (assert) {
	  var map = new SourceMapGenerator({
	    sourceRoot: 'http://',
	    file: 'www.example.com/foo.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'www.example.com/original.js'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var sources = map.sources;
	  assert.equal(sources.length, 1);
	  assert.equal(sources[0], 'http://www.example.com/original.js');
	};
	
	// Was github issue #43, but that's no longer valid.
	exports['test source resolution with sourceMapURL'] = function (assert) {
	  var map = new SourceMapGenerator({
	    sourceRoot: '',
	    file: 'foo.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'original.js',
	  });
	  map = new SourceMapConsumer(map.toString(), 'http://cdn.example.com');
	
	  var sources = map.sources;
	  assert.equal(sources.length, 1,
	               'Should only be one source.');
	  assert.equal(sources[0], 'http://cdn.example.com/original.js',
	               'Should be joined with the source map URL.');
	};
	
	exports['test sourceRoot prepending'] = function (assert) {
	  var map = new SourceMapGenerator({
	    sourceRoot: 'http://example.com/foo/bar',
	    file: 'foo.js'
	  });
	  map.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: '/original.js'
	  });
	  map = new SourceMapConsumer(map.toString());
	
	  var sources = map.sources;
	  assert.equal(sources.length, 1,
	               'Should only be one source.');
	  assert.equal(sources[0], 'http://example.com/foo/bar/original.js',
	               'Source include the source root.');
	};
	
	exports['test indexed source map errors when sections are out of order by line'] = function(assert) {
	  // Make a deep copy of the indexedTestMap
	  var misorderedIndexedTestMap = JSON.parse(JSON.stringify(util.indexedTestMap));
	
	  misorderedIndexedTestMap.sections[0].offset = {
	    line: 2,
	    column: 0
	  };
	
	  assert.throws(function() {
	    new SourceMapConsumer(misorderedIndexedTestMap);
	  }, Error);
	};
	
	exports['test github issue #64'] = function (assert) {
	  var map = new SourceMapConsumer({
	    "version": 3,
	    "file": "foo.js",
	    "sourceRoot": "http://example.com/",
	    "sources": ["/a"],
	    "names": [],
	    "mappings": "AACA",
	    "sourcesContent": ["foo"]
	  });
	
	  assert.equal(map.sourceContentFor("a"), "foo");
	  assert.equal(map.sourceContentFor("/a"), "foo");
	};
	
	exports['test full source content with sourceMapURL'] = function (assert) {
	  var map = new SourceMapConsumer({
	    'version': 3,
	    'file': 'foo.js',
	    'sourceRoot': '',
	    'sources': ['original.js'],
	    'names': [],
	    'mappings': 'AACA',
	    'sourcesContent': ['yellow warbler']
	  }, 'http://cdn.example.com');
	
	  var sources = map.sources;
	  assert.equal(map.sourceContentFor('http://cdn.example.com/original.js'), 'yellow warbler',
	               'Source content should be found using full URL');
	};
	
	exports['test bug 885597'] = function (assert) {
	  var map = new SourceMapConsumer({
	    "version": 3,
	    "file": "foo.js",
	    "sourceRoot": "file:///Users/AlGore/Invented/The/Internet/",
	    "sources": ["/a"],
	    "names": [],
	    "mappings": "AACA",
	    "sourcesContent": ["foo"]
	  });
	
	  var s = map.sources[0];
	  assert.equal(map.sourceContentFor(s), "foo");
	};
	
	exports['test github issue #72, duplicate sources'] = function (assert) {
	  var map = new SourceMapConsumer({
	    "version": 3,
	    "file": "foo.js",
	    "sources": ["source1.js", "source1.js", "source3.js"],
	    "names": [],
	    "mappings": ";EAAC;;IAEE;;MEEE",
	    "sourceRoot": "http://example.com"
	  });
	
	  var pos = map.originalPositionFor({
	    line: 2,
	    column: 2
	  });
	  assert.equal(pos.source, 'http://example.com/source1.js');
	  assert.equal(pos.line, 1);
	  assert.equal(pos.column, 1);
	
	  var pos = map.originalPositionFor({
	    line: 4,
	    column: 4
	  });
	  assert.equal(pos.source, 'http://example.com/source1.js');
	  assert.equal(pos.line, 3);
	  assert.equal(pos.column, 3);
	
	  var pos = map.originalPositionFor({
	    line: 6,
	    column: 6
	  });
	  assert.equal(pos.source, 'http://example.com/source3.js');
	  assert.equal(pos.line, 5);
	  assert.equal(pos.column, 5);
	};
	
	exports['test github issue #72, duplicate names'] = function (assert) {
	  var map = new SourceMapConsumer({
	    "version": 3,
	    "file": "foo.js",
	    "sources": ["source.js"],
	    "names": ["name1", "name1", "name3"],
	    "mappings": ";EAACA;;IAEEA;;MAEEE",
	    "sourceRoot": "http://example.com"
	  });
	
	  var pos = map.originalPositionFor({
	    line: 2,
	    column: 2
	  });
	  assert.equal(pos.name, 'name1');
	  assert.equal(pos.line, 1);
	  assert.equal(pos.column, 1);
	
	  var pos = map.originalPositionFor({
	    line: 4,
	    column: 4
	  });
	  assert.equal(pos.name, 'name1');
	  assert.equal(pos.line, 3);
	  assert.equal(pos.column, 3);
	
	  var pos = map.originalPositionFor({
	    line: 6,
	    column: 6
	  });
	  assert.equal(pos.name, 'name3');
	  assert.equal(pos.line, 5);
	  assert.equal(pos.column, 5);
	};
	
	exports['test SourceMapConsumer.fromSourceMap'] = function (assert) {
	  var smg = new SourceMapGenerator({
	    sourceRoot: 'http://example.com/',
	    file: 'foo.js'
	  });
	  smg.addMapping({
	    original: { line: 1, column: 1 },
	    generated: { line: 2, column: 2 },
	    source: 'bar.js'
	  });
	  smg.addMapping({
	    original: { line: 2, column: 2 },
	    generated: { line: 4, column: 4 },
	    source: 'baz.js',
	    name: 'dirtMcGirt'
	  });
	  smg.setSourceContent('baz.js', 'baz.js content');
	
	  var smc = SourceMapConsumer.fromSourceMap(smg);
	  assert.equal(smc.file, 'foo.js');
	  assert.equal(smc.sourceRoot, 'http://example.com/');
	  assert.equal(smc.sources.length, 2);
	  assert.equal(smc.sources[0], 'http://example.com/bar.js');
	  assert.equal(smc.sources[1], 'http://example.com/baz.js');
	  assert.equal(smc.sourceContentFor('baz.js'), 'baz.js content');
	
	  var pos = smc.originalPositionFor({
	    line: 2,
	    column: 2
	  });
	  assert.equal(pos.line, 1);
	  assert.equal(pos.column, 1);
	  assert.equal(pos.source, 'http://example.com/bar.js');
	  assert.equal(pos.name, null);
	
	  pos = smc.generatedPositionFor({
	    line: 1,
	    column: 1,
	    source: 'http://example.com/bar.js'
	  });
	  assert.equal(pos.line, 2);
	  assert.equal(pos.column, 2);
	
	  pos = smc.originalPositionFor({
	    line: 4,
	    column: 4
	  });
	  assert.equal(pos.line, 2);
	  assert.equal(pos.column, 2);
	  assert.equal(pos.source, 'http://example.com/baz.js');
	  assert.equal(pos.name, 'dirtMcGirt');
	
	  pos = smc.generatedPositionFor({
	    line: 2,
	    column: 2,
	    source: 'http://example.com/baz.js'
	  });
	  assert.equal(pos.line, 4);
	  assert.equal(pos.column, 4);
	};
	
	exports['test issue #191'] = function (assert) {
	  var generator = new SourceMapGenerator({ file: 'a.css' });
	  generator.addMapping({
	    source:   'b.css',
	    original: {
	      line:   1,
	      column: 0
	    },
	    generated: {
	      line:   1,
	      column: 0
	    }
	  });
	
	  // Create a SourceMapConsumer from the SourceMapGenerator, ...
	  var consumer  = SourceMapConsumer.fromSourceMap(generator);
	  // ... and then try and use the SourceMapGenerator again. This should not
	  // throw.
	  generator.toJSON();
	
	  assert.ok(true, "Using a SourceMapGenerator again after creating a " +
	                  "SourceMapConsumer from it should not throw");
	};
	
	exports['test sources where their prefix is the source root: issue #199'] = function (assert) {
	  var testSourceMap = {
	    "version": 3,
	    "sources": ["/source/app/app/app.js"],
	    "names": ["System"],
	    "mappings": "AAAAA",
	    "file": "app/app.js",
	    "sourcesContent": ["'use strict';"],
	    "sourceRoot":"/source/"
	  };
	
	  var consumer = new SourceMapConsumer(testSourceMap);
	
	  function consumerHasSource(s) {
	    assert.ok(consumer.sourceContentFor(s));
	  }
	
	  consumer.sources.forEach(consumerHasSource);
	  testSourceMap.sources.forEach(consumerHasSource);
	};
	
	exports['test sources where their prefix is the source root and the source root is a url: issue #199'] = function (assert) {
	  var testSourceMap = {
	    "version": 3,
	    "sources": ["http://example.com/source/app/app/app.js"],
	    "names": ["System"],
	    "mappings": "AAAAA",
	    "sourcesContent": ["'use strict';"],
	    "sourceRoot":"http://example.com/source/"
	  };
	
	  var consumer = new SourceMapConsumer(testSourceMap);
	
	  function consumerHasSource(s) {
	    assert.ok(consumer.sourceContentFor(s));
	  }
	
	  consumer.sources.forEach(consumerHasSource);
	  testSourceMap.sources.forEach(consumerHasSource);
	};
	
	exports['test consuming names and sources that are numbers'] = function (assert) {
	  var testSourceMap = {
	    "version": 3,
	    "sources": [0],
	    "names": [1],
	    "mappings": "AAAAA",
	  };
	
	  var consumer = new SourceMapConsumer(testSourceMap);
	
	  assert.equal(consumer.sources.length, 1);
	  assert.equal(consumer.sources[0], "0");
	
	  var i = 0;
	  consumer.eachMapping(function (m) {
	    i++;
	    assert.equal(m.name, "1");
	  });
	  assert.equal(i, 1);
	};
	
	exports['test non-normalized sourceRoot (from issue #227)'] = function (assert) {
	  var consumer = new SourceMapConsumer({
	    version: 3,
	    sources: [ 'index.js' ],
	    names: [],
	    mappings: ';;AAAA,IAAI,OAAO,MAAP',
	    file: 'index.js',
	    sourceRoot: './src/',
	    sourcesContent: [ 'var name = "Mark"\n' ]
	  });
	  assert.equal(consumer.sourceRoot, 'src/', 'sourceRoot was normalized');
	  // Before the fix, this threw an exception.
	  consumer.sourceContentFor(consumer.sources[0]);
	};
	
	exports['test webpack URL resolution'] = function (assert) {
	  var map = {
	    version: 3,
	    sources:  ["webpack:///webpack/bootstrap 67e184f9679733298d44"],
	    names: [],
	    mappings: "CAAS",
	    file: "static/js/manifest.b7cf97680f7a50fa150f.js",
	    sourceRoot: ""
	  };
	  var consumer = new SourceMapConsumer(map);
	
	  assert.equal(consumer.sources.length, 1);
	  assert.equal(consumer.sources[0], "webpack:///webpack/bootstrap 67e184f9679733298d44");
	};
	
	exports['test webpack URL resolution with sourceMapURL'] = function (assert) {
	  var map = {
	    version: 3,
	    sources:  ["webpack:///webpack/bootstrap 67e184f9679733298d44"],
	    names: [],
	    mappings: "CAAS",
	    file: "static/js/manifest.b7cf97680f7a50fa150f.js",
	    sourceRoot: ""
	  };
	  var consumer = new SourceMapConsumer(map, 'http://www.example.com/q.js.map');
	
	  assert.equal(consumer.sources.length, 1);
	  assert.equal(consumer.sources[0], "webpack:///webpack/bootstrap 67e184f9679733298d44");
	};
	
	exports['test relative webpack URL resolution with sourceMapURL'] = function (assert) {
	  var map = {
	    version: 3,
	    sources:  ["webpack/bootstrap.js"],
	    names: [],
	    mappings: "CAAS",
	    file: "static/js/manifest.b7cf97680f7a50fa150f.js",
	    sourceRoot: "webpack:///"
	  };
	  var consumer = new SourceMapConsumer(map, 'http://www.example.com/q.js.map');
	
	  assert.equal(consumer.sources.length, 1);
	  assert.equal(consumer.sources[0], "webpack:///webpack/bootstrap.js");
	};
	
	exports['test basic URL resolution with sourceMapURL'] = function (assert) {
	  var map = {
	    version: 3,
	    sources:  ["something.js"],
	    names: [],
	    mappings: "CAAS",
	    file: "static/js/manifest.b7cf97680f7a50fa150f.js",
	    sourceRoot: "src"
	  };
	  var consumer = new SourceMapConsumer(map, 'http://www.example.com/x/q.js.map');
	
	  assert.equal(consumer.sources.length, 1);
	  assert.equal(consumer.sources[0], 'http://www.example.com/x/src/something.js');
	};
	
	exports['test absolute sourceURL resolution with sourceMapURL'] = function (assert) {
	  var map = {
	    version: 3,
	    sources:  ["something.js"],
	    names: [],
	    mappings: "CAAS",
	    file: "static/js/manifest.b7cf97680f7a50fa150f.js",
	    sourceRoot: "http://www.example.com/src"
	  };
	  var consumer = new SourceMapConsumer(map, 'http://www.example.com/x/q.js.map');
	
	  assert.equal(consumer.sources.length, 1);
	  assert.equal(consumer.sources[0], 'http://www.example.com/src/something.js');
	};


/***/ }),
/* 1 */
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(2);
	
	// This is a test mapping which maps functions from two different files
	// (one.js and two.js) to a minified generated source.
	//
	// Here is one.js:
	//
	//   ONE.foo = function (bar) {
	//     return baz(bar);
	//   };
	//
	// Here is two.js:
	//
	//   TWO.inc = function (n) {
	//     return n + 1;
	//   };
	//
	// And here is the generated code (min.js):
	//
	//   ONE.foo=function(a){return baz(a);};
	//   TWO.inc=function(a){return a+1;};
	exports.testGeneratedCode = " ONE.foo=function(a){return baz(a);};\n"+
	                            " TWO.inc=function(a){return a+1;};";
	exports.testMap = {
	  version: 3,
	  file: 'min.js',
	  names: ['bar', 'baz', 'n'],
	  sources: ['one.js', 'two.js'],
	  sourceRoot: '/the/root',
	  mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	};
	exports.testMapNoSourceRoot = {
	  version: 3,
	  file: 'min.js',
	  names: ['bar', 'baz', 'n'],
	  sources: ['one.js', 'two.js'],
	  mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	};
	exports.testMapEmptySourceRoot = {
	  version: 3,
	  file: 'min.js',
	  names: ['bar', 'baz', 'n'],
	  sources: ['one.js', 'two.js'],
	  sourceRoot: '',
	  mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	};
	exports.testMapSingleSource = {
	  version: 3,
	  file: 'min.js',
	  names: ['bar', 'baz'],
	  sources: ['one.js'],
	  sourceRoot: '',
	  mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID'
	};
	exports.testMapEmptyMappings = {
	  version: 3,
	  file: 'min.js',
	  names: [],
	  sources: ['one.js', 'two.js'],
	  sourcesContent: [
	    ' ONE.foo = 1;',
	    ' TWO.inc = 2;'
	  ],
	  sourceRoot: '',
	  mappings: ''
	};
	exports.testMapEmptyMappingsRelativeSources = {
	  version: 3,
	  file: 'min.js',
	  names: [],
	  sources: ['./one.js', './two.js'],
	  sourcesContent: [
	    ' ONE.foo = 1;',
	    ' TWO.inc = 2;'
	  ],
	  sourceRoot: '/the/root',
	  mappings: ''
	};
	exports.testMapEmptyMappingsRelativeSources_generatedExpected = {
	  version: 3,
	  file: 'min.js',
	  names: [],
	  sources: ['one.js', 'two.js'],
	  sourcesContent: [
	    ' ONE.foo = 1;',
	    ' TWO.inc = 2;'
	  ],
	  sourceRoot: '/the/root',
	  mappings: ''
	};
	exports.testMapMultiSourcesMappingRefersSingleSourceOnly = {
	    version: 3,
	    file: 'min.js',
	    names: ['bar', 'baz'],
	    sources: ['one.js', 'withoutMappings.js'],
	    sourceRoot: '',
	    mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID'
	};
	// This mapping is identical to above, but uses the indexed format instead.
	exports.indexedTestMap = {
	  version: 3,
	  file: 'min.js',
	  sections: [
	    {
	      offset: {
	        line: 0,
	        column: 0
	      },
	      map: {
	        version: 3,
	        sources: [
	          "one.js"
	        ],
	        sourcesContent: [
	          ' ONE.foo = function (bar) {\n' +
	          '   return baz(bar);\n' +
	          ' };',
	        ],
	        names: [
	          "bar",
	          "baz"
	        ],
	        mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID",
	        file: "min.js",
	        sourceRoot: "/the/root"
	      }
	    },
	    {
	      offset: {
	        line: 1,
	        column: 0
	      },
	      map: {
	        version: 3,
	        sources: [
	          "two.js"
	        ],
	        sourcesContent: [
	          ' TWO.inc = function (n) {\n' +
	          '   return n + 1;\n' +
	          ' };'
	        ],
	        names: [
	          "n"
	        ],
	        mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOA",
	        file: "min.js",
	        sourceRoot: "/the/root"
	      }
	    }
	  ]
	};
	exports.indexedTestMapDifferentSourceRoots = {
	  version: 3,
	  file: 'min.js',
	  sections: [
	    {
	      offset: {
	        line: 0,
	        column: 0
	      },
	      map: {
	        version: 3,
	        sources: [
	          "one.js"
	        ],
	        sourcesContent: [
	          ' ONE.foo = function (bar) {\n' +
	          '   return baz(bar);\n' +
	          ' };',
	        ],
	        names: [
	          "bar",
	          "baz"
	        ],
	        mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID",
	        file: "min.js",
	        sourceRoot: "/the/root"
	      }
	    },
	    {
	      offset: {
	        line: 1,
	        column: 0
	      },
	      map: {
	        version: 3,
	        sources: [
	          "two.js"
	        ],
	        sourcesContent: [
	          ' TWO.inc = function (n) {\n' +
	          '   return n + 1;\n' +
	          ' };'
	        ],
	        names: [
	          "n"
	        ],
	        mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOA",
	        file: "min.js",
	        sourceRoot: "/different/root"
	      }
	    }
	  ]
	};
	exports.testMapWithSourcesContent = {
	  version: 3,
	  file: 'min.js',
	  names: ['bar', 'baz', 'n'],
	  sources: ['one.js', 'two.js'],
	  sourcesContent: [
	    ' ONE.foo = function (bar) {\n' +
	    '   return baz(bar);\n' +
	    ' };',
	    ' TWO.inc = function (n) {\n' +
	    '   return n + 1;\n' +
	    ' };'
	  ],
	  sourceRoot: '/the/root',
	  mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	};
	exports.testMapRelativeSources = {
	  version: 3,
	  file: 'min.js',
	  names: ['bar', 'baz', 'n'],
	  sources: ['./one.js', './two.js'],
	  sourcesContent: [
	    ' ONE.foo = function (bar) {\n' +
	    '   return baz(bar);\n' +
	    ' };',
	    ' TWO.inc = function (n) {\n' +
	    '   return n + 1;\n' +
	    ' };'
	  ],
	  sourceRoot: '/the/root',
	  mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	};
	exports.emptyMap = {
	  version: 3,
	  file: 'min.js',
	  names: [],
	  sources: [],
	  mappings: ''
	};
	
	
	function assertMapping(generatedLine, generatedColumn, originalSource,
	                       originalLine, originalColumn, name, bias, map, assert,
	                       dontTestGenerated, dontTestOriginal) {
	  if (!dontTestOriginal) {
	    var origMapping = map.originalPositionFor({
	      line: generatedLine,
	      column: generatedColumn,
	      bias: bias
	    });
	    assert.equal(origMapping.name, name,
	                 'Incorrect name, expected ' + JSON.stringify(name)
	                 + ', got ' + JSON.stringify(origMapping.name));
	    assert.equal(origMapping.line, originalLine,
	                 'Incorrect line, expected ' + JSON.stringify(originalLine)
	                 + ', got ' + JSON.stringify(origMapping.line));
	    assert.equal(origMapping.column, originalColumn,
	                 'Incorrect column, expected ' + JSON.stringify(originalColumn)
	                 + ', got ' + JSON.stringify(origMapping.column));
	
	    var expectedSource;
	
	    if (originalSource && map.sourceRoot && originalSource.indexOf(map.sourceRoot) === 0) {
	      expectedSource = originalSource;
	    } else if (originalSource) {
	      expectedSource = map.sourceRoot
	        ? util.join(map.sourceRoot, originalSource)
	        : originalSource;
	    } else {
	      expectedSource = null;
	    }
	
	    assert.equal(origMapping.source, expectedSource,
	                 'Incorrect source, expected ' + JSON.stringify(expectedSource)
	                 + ', got ' + JSON.stringify(origMapping.source));
	  }
	
	  if (!dontTestGenerated) {
	    var genMapping = map.generatedPositionFor({
	      source: originalSource,
	      line: originalLine,
	      column: originalColumn,
	      bias: bias
	    });
	    assert.equal(genMapping.line, generatedLine,
	                 'Incorrect line, expected ' + JSON.stringify(generatedLine)
	                 + ', got ' + JSON.stringify(genMapping.line));
	    assert.equal(genMapping.column, generatedColumn,
	                 'Incorrect column, expected ' + JSON.stringify(generatedColumn)
	                 + ', got ' + JSON.stringify(genMapping.column));
	  }
	}
	exports.assertMapping = assertMapping;
	
	function assertEqualMaps(assert, actualMap, expectedMap) {
	  assert.equal(actualMap.version, expectedMap.version, "version mismatch");
	  assert.equal(actualMap.file, expectedMap.file, "file mismatch");
	  assert.equal(actualMap.names.length,
	               expectedMap.names.length,
	               "names length mismatch: " +
	                 actualMap.names.join(", ") + " != " + expectedMap.names.join(", "));
	  for (var i = 0; i < actualMap.names.length; i++) {
	    assert.equal(actualMap.names[i],
	                 expectedMap.names[i],
	                 "names[" + i + "] mismatch: " +
	                   actualMap.names.join(", ") + " != " + expectedMap.names.join(", "));
	  }
	  assert.equal(actualMap.sources.length,
	               expectedMap.sources.length,
	               "sources length mismatch: " +
	                 actualMap.sources.join(", ") + " != " + expectedMap.sources.join(", "));
	  for (var i = 0; i < actualMap.sources.length; i++) {
	    assert.equal(actualMap.sources[i],
	                 expectedMap.sources[i],
	                 "sources[" + i + "] length mismatch: " +
	                 actualMap.sources.join(", ") + " != " + expectedMap.sources.join(", "));
	  }
	  assert.equal(actualMap.sourceRoot,
	               expectedMap.sourceRoot,
	               "sourceRoot mismatch: " +
	                 actualMap.sourceRoot + " != " + expectedMap.sourceRoot);
	  assert.equal(actualMap.mappings, expectedMap.mappings,
	               "mappings mismatch:\nActual:   " + actualMap.mappings + "\nExpected: " + expectedMap.mappings);
	  if (actualMap.sourcesContent) {
	    assert.equal(actualMap.sourcesContent.length,
	                 expectedMap.sourcesContent.length,
	                 "sourcesContent length mismatch");
	    for (var i = 0; i < actualMap.sourcesContent.length; i++) {
	      assert.equal(actualMap.sourcesContent[i],
	                   expectedMap.sourcesContent[i],
	                   "sourcesContent[" + i + "] mismatch");
	    }
	  }
	}
	exports.assertEqualMaps = assertEqualMaps;


/***/ }),
/* 2 */
/***/ (function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	/**
	 * This is a helper function for getting values from parameter/options
	 * objects.
	 *
	 * @param args The object we are extracting values from
	 * @param name The name of the property we are getting.
	 * @param defaultValue An optional value to return if the property is missing
	 * from the object. If this is not specified and the property is missing, an
	 * error will be thrown.
	 */
	function getArg(aArgs, aName, aDefaultValue) {
	  if (aName in aArgs) {
	    return aArgs[aName];
	  } else if (arguments.length === 3) {
	    return aDefaultValue;
	  } else {
	    throw new Error('"' + aName + '" is a required argument.');
	  }
	}
	exports.getArg = getArg;
	
	var urlRegexp = /^(?:([\w+\-.]+):)?\/\/(?:(\w+:\w+)@)?([\w.-]*)(?::(\d+))?(.*)$/;
	var dataUrlRegexp = /^data:.+\,.+$/;
	
	function urlParse(aUrl) {
	  var match = aUrl.match(urlRegexp);
	  if (!match) {
	    return null;
	  }
	  return {
	    scheme: match[1],
	    auth: match[2],
	    host: match[3],
	    port: match[4],
	    path: match[5]
	  };
	}
	exports.urlParse = urlParse;
	
	function urlGenerate(aParsedUrl) {
	  var url = '';
	  if (aParsedUrl.scheme) {
	    url += aParsedUrl.scheme + ':';
	  }
	  url += '//';
	  if (aParsedUrl.auth) {
	    url += aParsedUrl.auth + '@';
	  }
	  if (aParsedUrl.host) {
	    url += aParsedUrl.host;
	  }
	  if (aParsedUrl.port) {
	    url += ":" + aParsedUrl.port
	  }
	  if (aParsedUrl.path) {
	    url += aParsedUrl.path;
	  }
	  return url;
	}
	exports.urlGenerate = urlGenerate;
	
	/**
	 * Normalizes a path, or the path portion of a URL:
	 *
	 * - Replaces consecutive slashes with one slash.
	 * - Removes unnecessary '.' parts.
	 * - Removes unnecessary '<dir>/..' parts.
	 *
	 * Based on code in the Node.js 'path' core module.
	 *
	 * @param aPath The path or url to normalize.
	 */
	function normalize(aPath) {
	  var path = aPath;
	  var url = urlParse(aPath);
	  if (url) {
	    if (!url.path) {
	      return aPath;
	    }
	    path = url.path;
	  }
	  var isAbsolute = exports.isAbsolute(path);
	
	  var parts = path.split(/\/+/);
	  for (var part, up = 0, i = parts.length - 1; i >= 0; i--) {
	    part = parts[i];
	    if (part === '.') {
	      parts.splice(i, 1);
	    } else if (part === '..') {
	      up++;
	    } else if (up > 0) {
	      if (part === '') {
	        // The first part is blank if the path is absolute. Trying to go
	        // above the root is a no-op. Therefore we can remove all '..' parts
	        // directly after the root.
	        parts.splice(i + 1, up);
	        up = 0;
	      } else {
	        parts.splice(i, 2);
	        up--;
	      }
	    }
	  }
	  path = parts.join('/');
	
	  if (path === '') {
	    path = isAbsolute ? '/' : '.';
	  }
	
	  if (url) {
	    url.path = path;
	    return urlGenerate(url);
	  }
	  return path;
	}
	exports.normalize = normalize;
	
	/**
	 * Joins two paths/URLs.
	 *
	 * @param aRoot The root path or URL.
	 * @param aPath The path or URL to be joined with the root.
	 *
	 * - If aPath is a URL or a data URI, aPath is returned, unless aPath is a
	 *   scheme-relative URL: Then the scheme of aRoot, if any, is prepended
	 *   first.
	 * - Otherwise aPath is a path. If aRoot is a URL, then its path portion
	 *   is updated with the result and aRoot is returned. Otherwise the result
	 *   is returned.
	 *   - If aPath is absolute, the result is aPath.
	 *   - Otherwise the two paths are joined with a slash.
	 * - Joining for example 'http://' and 'www.example.com' is also supported.
	 */
	function join(aRoot, aPath) {
	  if (aRoot === "") {
	    aRoot = ".";
	  }
	  if (aPath === "") {
	    aPath = ".";
	  }
	  var aPathUrl = urlParse(aPath);
	  var aRootUrl = urlParse(aRoot);
	  if (aRootUrl) {
	    aRoot = aRootUrl.path || '/';
	  }
	
	  // `join(foo, '//www.example.org')`
	  if (aPathUrl && !aPathUrl.scheme) {
	    if (aRootUrl) {
	      aPathUrl.scheme = aRootUrl.scheme;
	    }
	    return urlGenerate(aPathUrl);
	  }
	
	  if (aPathUrl || aPath.match(dataUrlRegexp)) {
	    return aPath;
	  }
	
	  // `join('http://', 'www.example.com')`
	  if (aRootUrl && !aRootUrl.host && !aRootUrl.path) {
	    aRootUrl.host = aPath;
	    return urlGenerate(aRootUrl);
	  }
	
	  var joined = aPath.charAt(0) === '/'
	    ? aPath
	    : normalize(aRoot.replace(/\/+$/, '') + '/' + aPath);
	
	  if (aRootUrl) {
	    aRootUrl.path = joined;
	    return urlGenerate(aRootUrl);
	  }
	  return joined;
	}
	exports.join = join;
	
	exports.isAbsolute = function (aPath) {
	  return aPath.charAt(0) === '/' || urlRegexp.test(aPath);
	};
	
	/**
	 * Make a path relative to a URL or another path.
	 *
	 * @param aRoot The root path or URL.
	 * @param aPath The path or URL to be made relative to aRoot.
	 */
	function relative(aRoot, aPath) {
	  if (aRoot === "") {
	    aRoot = ".";
	  }
	
	  aRoot = aRoot.replace(/\/$/, '');
	
	  // It is possible for the path to be above the root. In this case, simply
	  // checking whether the root is a prefix of the path won't work. Instead, we
	  // need to remove components from the root one by one, until either we find
	  // a prefix that fits, or we run out of components to remove.
	  var level = 0;
	  while (aPath.indexOf(aRoot + '/') !== 0) {
	    var index = aRoot.lastIndexOf("/");
	    if (index < 0) {
	      return aPath;
	    }
	
	    // If the only part of the root that is left is the scheme (i.e. http://,
	    // file:///, etc.), one or more slashes (/), or simply nothing at all, we
	    // have exhausted all components, so the path is not relative to the root.
	    aRoot = aRoot.slice(0, index);
	    if (aRoot.match(/^([^\/]+:\/)?\/*$/)) {
	      return aPath;
	    }
	
	    ++level;
	  }
	
	  // Make sure we add a "../" for each component we removed from the root.
	  return Array(level + 1).join("../") + aPath.substr(aRoot.length + 1);
	}
	exports.relative = relative;
	
	var supportsNullProto = (function () {
	  var obj = Object.create(null);
	  return !('__proto__' in obj);
	}());
	
	function identity (s) {
	  return s;
	}
	
	/**
	 * Because behavior goes wacky when you set `__proto__` on objects, we
	 * have to prefix all the strings in our set with an arbitrary character.
	 *
	 * See https://github.com/mozilla/source-map/pull/31 and
	 * https://github.com/mozilla/source-map/issues/30
	 *
	 * @param String aStr
	 */
	function toSetString(aStr) {
	  if (isProtoString(aStr)) {
	    return '$' + aStr;
	  }
	
	  return aStr;
	}
	exports.toSetString = supportsNullProto ? identity : toSetString;
	
	function fromSetString(aStr) {
	  if (isProtoString(aStr)) {
	    return aStr.slice(1);
	  }
	
	  return aStr;
	}
	exports.fromSetString = supportsNullProto ? identity : fromSetString;
	
	function isProtoString(s) {
	  if (!s) {
	    return false;
	  }
	
	  var length = s.length;
	
	  if (length < 9 /* "__proto__".length */) {
	    return false;
	  }
	
	  if (s.charCodeAt(length - 1) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 2) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 3) !== 111 /* 'o' */ ||
	      s.charCodeAt(length - 4) !== 116 /* 't' */ ||
	      s.charCodeAt(length - 5) !== 111 /* 'o' */ ||
	      s.charCodeAt(length - 6) !== 114 /* 'r' */ ||
	      s.charCodeAt(length - 7) !== 112 /* 'p' */ ||
	      s.charCodeAt(length - 8) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 9) !== 95  /* '_' */) {
	    return false;
	  }
	
	  for (var i = length - 10; i >= 0; i--) {
	    if (s.charCodeAt(i) !== 36 /* '$' */) {
	      return false;
	    }
	  }
	
	  return true;
	}
	
	/**
	 * Comparator between two mappings where the original positions are compared.
	 *
	 * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	 * mappings with the same original source/line/column, but different generated
	 * line and column the same. Useful when searching for a mapping with a
	 * stubbed out mapping.
	 */
	function compareByOriginalPositions(mappingA, mappingB, onlyCompareOriginal) {
	  var cmp = strcmp(mappingA.source, mappingB.source);
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalLine - mappingB.originalLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalColumn - mappingB.originalColumn;
	  if (cmp !== 0 || onlyCompareOriginal) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedLine - mappingB.generatedLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  return strcmp(mappingA.name, mappingB.name);
	}
	exports.compareByOriginalPositions = compareByOriginalPositions;
	
	/**
	 * Comparator between two mappings with deflated source and name indices where
	 * the generated positions are compared.
	 *
	 * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	 * mappings with the same generated line and column, but different
	 * source/name/original line and column the same. Useful when searching for a
	 * mapping with a stubbed out mapping.
	 */
	function compareByGeneratedPositionsDeflated(mappingA, mappingB, onlyCompareGenerated) {
	  var cmp = mappingA.generatedLine - mappingB.generatedLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	  if (cmp !== 0 || onlyCompareGenerated) {
	    return cmp;
	  }
	
	  cmp = strcmp(mappingA.source, mappingB.source);
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalLine - mappingB.originalLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalColumn - mappingB.originalColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  return strcmp(mappingA.name, mappingB.name);
	}
	exports.compareByGeneratedPositionsDeflated = compareByGeneratedPositionsDeflated;
	
	function strcmp(aStr1, aStr2) {
	  if (aStr1 === aStr2) {
	    return 0;
	  }
	
	  if (aStr1 === null) {
	    return 1; // aStr2 !== null
	  }
	
	  if (aStr2 === null) {
	    return -1; // aStr1 !== null
	  }
	
	  if (aStr1 > aStr2) {
	    return 1;
	  }
	
	  return -1;
	}
	
	/**
	 * Comparator between two mappings with inflated source and name strings where
	 * the generated positions are compared.
	 */
	function compareByGeneratedPositionsInflated(mappingA, mappingB) {
	  var cmp = mappingA.generatedLine - mappingB.generatedLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = strcmp(mappingA.source, mappingB.source);
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalLine - mappingB.originalLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalColumn - mappingB.originalColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  return strcmp(mappingA.name, mappingB.name);
	}
	exports.compareByGeneratedPositionsInflated = compareByGeneratedPositionsInflated;
	
	/**
	 * Strip any JSON XSSI avoidance prefix from the string (as documented
	 * in the source maps specification), and then parse the string as
	 * JSON.
	 */
	function parseSourceMapInput(str) {
	  return JSON.parse(str.replace(/^\)]}'[^\n]*\n/, ''));
	}
	exports.parseSourceMapInput = parseSourceMapInput;
	
	/**
	 * Compute the URL of a source given the the source root, the source's
	 * URL, and the source map's URL.
	 */
	function computeSourceURL(sourceRoot, sourceURL, sourceMapURL) {
	  sourceURL = sourceURL || '';
	
	  if (sourceRoot) {
	    // This follows what Chrome does.
	    if (sourceRoot[sourceRoot.length - 1] !== '/' && sourceURL[0] !== '/') {
	      sourceRoot += '/';
	    }
	    // The spec says:
	    //   Line 4: An optional source root, useful for relocating source
	    //   files on a server or removing repeated values in the
	    //   “sources” entry.  This value is prepended to the individual
	    //   entries in the “source” field.
	    sourceURL = sourceRoot + sourceURL;
	  }
	
	  // Historically, SourceMapConsumer did not take the sourceMapURL as
	  // a parameter.  This mode is still somewhat supported, which is why
	  // this code block is conditional.  However, it's preferable to pass
	  // the source map URL to SourceMapConsumer, so that this function
	  // can implement the source URL resolution algorithm as outlined in
	  // the spec.  This block is basically the equivalent of:
	  //    new URL(sourceURL, sourceMapURL).toString()
	  // ... except it avoids using URL, which wasn't available in the
	  // older releases of node still supported by this library.
	  //
	  // The spec says:
	  //   If the sources are not absolute URLs after prepending of the
	  //   “sourceRoot”, the sources are resolved relative to the
	  //   SourceMap (like resolving script src in a html document).
	  if (sourceMapURL) {
	    var parsed = urlParse(sourceMapURL);
	    if (!parsed) {
	      throw new Error("sourceMapURL could not be parsed");
	    }
	    if (parsed.path) {
	      // Strip the last path component, but keep the "/".
	      var index = parsed.path.lastIndexOf('/');
	      if (index >= 0) {
	        parsed.path = parsed.path.substring(0, index + 1);
	      }
	    }
	    sourceURL = join(urlGenerate(parsed), sourceURL);
	  }
	
	  return normalize(sourceURL);
	}
	exports.computeSourceURL = computeSourceURL;


/***/ }),
/* 3 */
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(2);
	var binarySearch = __webpack_require__(4);
	var ArraySet = __webpack_require__(5).ArraySet;
	var base64VLQ = __webpack_require__(6);
	var quickSort = __webpack_require__(8).quickSort;
	
	function SourceMapConsumer(aSourceMap, aSourceMapURL) {
	  var sourceMap = aSourceMap;
	  if (typeof aSourceMap === 'string') {
	    sourceMap = util.parseSourceMapInput(aSourceMap);
	  }
	
	  return sourceMap.sections != null
	    ? new IndexedSourceMapConsumer(sourceMap, aSourceMapURL)
	    : new BasicSourceMapConsumer(sourceMap, aSourceMapURL);
	}
	
	SourceMapConsumer.fromSourceMap = function(aSourceMap, aSourceMapURL) {
	  return BasicSourceMapConsumer.fromSourceMap(aSourceMap, aSourceMapURL);
	}
	
	/**
	 * The version of the source mapping spec that we are consuming.
	 */
	SourceMapConsumer.prototype._version = 3;
	
	// `__generatedMappings` and `__originalMappings` are arrays that hold the
	// parsed mapping coordinates from the source map's "mappings" attribute. They
	// are lazily instantiated, accessed via the `_generatedMappings` and
	// `_originalMappings` getters respectively, and we only parse the mappings
	// and create these arrays once queried for a source location. We jump through
	// these hoops because there can be many thousands of mappings, and parsing
	// them is expensive, so we only want to do it if we must.
	//
	// Each object in the arrays is of the form:
	//
	//     {
	//       generatedLine: The line number in the generated code,
	//       generatedColumn: The column number in the generated code,
	//       source: The path to the original source file that generated this
	//               chunk of code,
	//       originalLine: The line number in the original source that
	//                     corresponds to this chunk of generated code,
	//       originalColumn: The column number in the original source that
	//                       corresponds to this chunk of generated code,
	//       name: The name of the original symbol which generated this chunk of
	//             code.
	//     }
	//
	// All properties except for `generatedLine` and `generatedColumn` can be
	// `null`.
	//
	// `_generatedMappings` is ordered by the generated positions.
	//
	// `_originalMappings` is ordered by the original positions.
	
	SourceMapConsumer.prototype.__generatedMappings = null;
	Object.defineProperty(SourceMapConsumer.prototype, '_generatedMappings', {
	  configurable: true,
	  enumerable: true,
	  get: function () {
	    if (!this.__generatedMappings) {
	      this._parseMappings(this._mappings, this.sourceRoot);
	    }
	
	    return this.__generatedMappings;
	  }
	});
	
	SourceMapConsumer.prototype.__originalMappings = null;
	Object.defineProperty(SourceMapConsumer.prototype, '_originalMappings', {
	  configurable: true,
	  enumerable: true,
	  get: function () {
	    if (!this.__originalMappings) {
	      this._parseMappings(this._mappings, this.sourceRoot);
	    }
	
	    return this.__originalMappings;
	  }
	});
	
	SourceMapConsumer.prototype._charIsMappingSeparator =
	  function SourceMapConsumer_charIsMappingSeparator(aStr, index) {
	    var c = aStr.charAt(index);
	    return c === ";" || c === ",";
	  };
	
	/**
	 * Parse the mappings in a string in to a data structure which we can easily
	 * query (the ordered arrays in the `this.__generatedMappings` and
	 * `this.__originalMappings` properties).
	 */
	SourceMapConsumer.prototype._parseMappings =
	  function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	    throw new Error("Subclasses must implement _parseMappings");
	  };
	
	SourceMapConsumer.GENERATED_ORDER = 1;
	SourceMapConsumer.ORIGINAL_ORDER = 2;
	
	SourceMapConsumer.GREATEST_LOWER_BOUND = 1;
	SourceMapConsumer.LEAST_UPPER_BOUND = 2;
	
	/**
	 * Iterate over each mapping between an original source/line/column and a
	 * generated line/column in this source map.
	 *
	 * @param Function aCallback
	 *        The function that is called with each mapping.
	 * @param Object aContext
	 *        Optional. If specified, this object will be the value of `this` every
	 *        time that `aCallback` is called.
	 * @param aOrder
	 *        Either `SourceMapConsumer.GENERATED_ORDER` or
	 *        `SourceMapConsumer.ORIGINAL_ORDER`. Specifies whether you want to
	 *        iterate over the mappings sorted by the generated file's line/column
	 *        order or the original's source/line/column order, respectively. Defaults to
	 *        `SourceMapConsumer.GENERATED_ORDER`.
	 */
	SourceMapConsumer.prototype.eachMapping =
	  function SourceMapConsumer_eachMapping(aCallback, aContext, aOrder) {
	    var context = aContext || null;
	    var order = aOrder || SourceMapConsumer.GENERATED_ORDER;
	
	    var mappings;
	    switch (order) {
	    case SourceMapConsumer.GENERATED_ORDER:
	      mappings = this._generatedMappings;
	      break;
	    case SourceMapConsumer.ORIGINAL_ORDER:
	      mappings = this._originalMappings;
	      break;
	    default:
	      throw new Error("Unknown order of iteration.");
	    }
	
	    var sourceRoot = this.sourceRoot;
	    mappings.map(function (mapping) {
	      var source = mapping.source === null ? null : this._sources.at(mapping.source);
	      source = util.computeSourceURL(sourceRoot, source, this._sourceMapURL);
	      return {
	        source: source,
	        generatedLine: mapping.generatedLine,
	        generatedColumn: mapping.generatedColumn,
	        originalLine: mapping.originalLine,
	        originalColumn: mapping.originalColumn,
	        name: mapping.name === null ? null : this._names.at(mapping.name)
	      };
	    }, this).forEach(aCallback, context);
	  };
	
	/**
	 * Returns all generated line and column information for the original source,
	 * line, and column provided. If no column is provided, returns all mappings
	 * corresponding to a either the line we are searching for or the next
	 * closest line that has any mappings. Otherwise, returns all mappings
	 * corresponding to the given line and either the column we are searching for
	 * or the next closest column that has any offsets.
	 *
	 * The only argument is an object with the following properties:
	 *
	 *   - source: The filename of the original source.
	 *   - line: The line number in the original source.  The line number is 1-based.
	 *   - column: Optional. the column number in the original source.
	 *    The column number is 0-based.
	 *
	 * and an array of objects is returned, each with the following properties:
	 *
	 *   - line: The line number in the generated source, or null.  The
	 *    line number is 1-based.
	 *   - column: The column number in the generated source, or null.
	 *    The column number is 0-based.
	 */
	SourceMapConsumer.prototype.allGeneratedPositionsFor =
	  function SourceMapConsumer_allGeneratedPositionsFor(aArgs) {
	    var line = util.getArg(aArgs, 'line');
	
	    // When there is no exact match, BasicSourceMapConsumer.prototype._findMapping
	    // returns the index of the closest mapping less than the needle. By
	    // setting needle.originalColumn to 0, we thus find the last mapping for
	    // the given line, provided such a mapping exists.
	    var needle = {
	      source: util.getArg(aArgs, 'source'),
	      originalLine: line,
	      originalColumn: util.getArg(aArgs, 'column', 0)
	    };
	
	    needle.source = this._findSourceIndex(needle.source);
	    if (needle.source < 0) {
	      return [];
	    }
	
	    var mappings = [];
	
	    var index = this._findMapping(needle,
	                                  this._originalMappings,
	                                  "originalLine",
	                                  "originalColumn",
	                                  util.compareByOriginalPositions,
	                                  binarySearch.LEAST_UPPER_BOUND);
	    if (index >= 0) {
	      var mapping = this._originalMappings[index];
	
	      if (aArgs.column === undefined) {
	        var originalLine = mapping.originalLine;
	
	        // Iterate until either we run out of mappings, or we run into
	        // a mapping for a different line than the one we found. Since
	        // mappings are sorted, this is guaranteed to find all mappings for
	        // the line we found.
	        while (mapping && mapping.originalLine === originalLine) {
	          mappings.push({
	            line: util.getArg(mapping, 'generatedLine', null),
	            column: util.getArg(mapping, 'generatedColumn', null),
	            lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	          });
	
	          mapping = this._originalMappings[++index];
	        }
	      } else {
	        var originalColumn = mapping.originalColumn;
	
	        // Iterate until either we run out of mappings, or we run into
	        // a mapping for a different line than the one we were searching for.
	        // Since mappings are sorted, this is guaranteed to find all mappings for
	        // the line we are searching for.
	        while (mapping &&
	               mapping.originalLine === line &&
	               mapping.originalColumn == originalColumn) {
	          mappings.push({
	            line: util.getArg(mapping, 'generatedLine', null),
	            column: util.getArg(mapping, 'generatedColumn', null),
	            lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	          });
	
	          mapping = this._originalMappings[++index];
	        }
	      }
	    }
	
	    return mappings;
	  };
	
	exports.SourceMapConsumer = SourceMapConsumer;
	
	/**
	 * A BasicSourceMapConsumer instance represents a parsed source map which we can
	 * query for information about the original file positions by giving it a file
	 * position in the generated source.
	 *
	 * The first parameter is the raw source map (either as a JSON string, or
	 * already parsed to an object). According to the spec, source maps have the
	 * following attributes:
	 *
	 *   - version: Which version of the source map spec this map is following.
	 *   - sources: An array of URLs to the original source files.
	 *   - names: An array of identifiers which can be referrenced by individual mappings.
	 *   - sourceRoot: Optional. The URL root from which all sources are relative.
	 *   - sourcesContent: Optional. An array of contents of the original source files.
	 *   - mappings: A string of base64 VLQs which contain the actual mappings.
	 *   - file: Optional. The generated file this source map is associated with.
	 *
	 * Here is an example source map, taken from the source map spec[0]:
	 *
	 *     {
	 *       version : 3,
	 *       file: "out.js",
	 *       sourceRoot : "",
	 *       sources: ["foo.js", "bar.js"],
	 *       names: ["src", "maps", "are", "fun"],
	 *       mappings: "AA,AB;;ABCDE;"
	 *     }
	 *
	 * The second parameter, if given, is a string whose value is the URL
	 * at which the source map was found.  This URL is used to compute the
	 * sources array.
	 *
	 * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit?pli=1#
	 */
	function BasicSourceMapConsumer(aSourceMap, aSourceMapURL) {
	  var sourceMap = aSourceMap;
	  if (typeof aSourceMap === 'string') {
	    sourceMap = util.parseSourceMapInput(aSourceMap);
	  }
	
	  var version = util.getArg(sourceMap, 'version');
	  var sources = util.getArg(sourceMap, 'sources');
	  // Sass 3.3 leaves out the 'names' array, so we deviate from the spec (which
	  // requires the array) to play nice here.
	  var names = util.getArg(sourceMap, 'names', []);
	  var sourceRoot = util.getArg(sourceMap, 'sourceRoot', null);
	  var sourcesContent = util.getArg(sourceMap, 'sourcesContent', null);
	  var mappings = util.getArg(sourceMap, 'mappings');
	  var file = util.getArg(sourceMap, 'file', null);
	
	  // Once again, Sass deviates from the spec and supplies the version as a
	  // string rather than a number, so we use loose equality checking here.
	  if (version != this._version) {
	    throw new Error('Unsupported version: ' + version);
	  }
	
	  if (sourceRoot) {
	    sourceRoot = util.normalize(sourceRoot);
	  }
	
	  sources = sources
	    .map(String)
	    // Some source maps produce relative source paths like "./foo.js" instead of
	    // "foo.js".  Normalize these first so that future comparisons will succeed.
	    // See bugzil.la/1090768.
	    .map(util.normalize)
	    // Always ensure that absolute sources are internally stored relative to
	    // the source root, if the source root is absolute. Not doing this would
	    // be particularly problematic when the source root is a prefix of the
	    // source (valid, but why??). See github issue #199 and bugzil.la/1188982.
	    .map(function (source) {
	      return sourceRoot && util.isAbsolute(sourceRoot) && util.isAbsolute(source)
	        ? util.relative(sourceRoot, source)
	        : source;
	    });
	
	  // Pass `true` below to allow duplicate names and sources. While source maps
	  // are intended to be compressed and deduplicated, the TypeScript compiler
	  // sometimes generates source maps with duplicates in them. See Github issue
	  // #72 and bugzil.la/889492.
	  this._names = ArraySet.fromArray(names.map(String), true);
	  this._sources = ArraySet.fromArray(sources, true);
	
	  this._absoluteSources = this._sources.toArray().map(function (s) {
	    return util.computeSourceURL(sourceRoot, s, aSourceMapURL);
	  });
	
	  this.sourceRoot = sourceRoot;
	  this.sourcesContent = sourcesContent;
	  this._mappings = mappings;
	  this._sourceMapURL = aSourceMapURL;
	  this.file = file;
	}
	
	BasicSourceMapConsumer.prototype = Object.create(SourceMapConsumer.prototype);
	BasicSourceMapConsumer.prototype.consumer = SourceMapConsumer;
	
	/**
	 * Utility function to find the index of a source.  Returns -1 if not
	 * found.
	 */
	BasicSourceMapConsumer.prototype._findSourceIndex = function(aSource) {
	  var relativeSource = aSource;
	  if (this.sourceRoot != null) {
	    relativeSource = util.relative(this.sourceRoot, relativeSource);
	  }
	
	  if (this._sources.has(relativeSource)) {
	    return this._sources.indexOf(relativeSource);
	  }
	
	  // Maybe aSource is an absolute URL as returned by |sources|.  In
	  // this case we can't simply undo the transform.
	  var i;
	  for (i = 0; i < this._absoluteSources.length; ++i) {
	    if (this._absoluteSources[i] == aSource) {
	      return i;
	    }
	  }
	
	  return -1;
	};
	
	/**
	 * Create a BasicSourceMapConsumer from a SourceMapGenerator.
	 *
	 * @param SourceMapGenerator aSourceMap
	 *        The source map that will be consumed.
	 * @param String aSourceMapURL
	 *        The URL at which the source map can be found (optional)
	 * @returns BasicSourceMapConsumer
	 */
	BasicSourceMapConsumer.fromSourceMap =
	  function SourceMapConsumer_fromSourceMap(aSourceMap, aSourceMapURL) {
	    var smc = Object.create(BasicSourceMapConsumer.prototype);
	
	    var names = smc._names = ArraySet.fromArray(aSourceMap._names.toArray(), true);
	    var sources = smc._sources = ArraySet.fromArray(aSourceMap._sources.toArray(), true);
	    smc.sourceRoot = aSourceMap._sourceRoot;
	    smc.sourcesContent = aSourceMap._generateSourcesContent(smc._sources.toArray(),
	                                                            smc.sourceRoot);
	    smc.file = aSourceMap._file;
	    smc._sourceMapURL = aSourceMapURL;
	    smc._absoluteSources = smc._sources.toArray().map(function (s) {
	      return util.computeSourceURL(smc.sourceRoot, s, aSourceMapURL);
	    });
	
	    // Because we are modifying the entries (by converting string sources and
	    // names to indices into the sources and names ArraySets), we have to make
	    // a copy of the entry or else bad things happen. Shared mutable state
	    // strikes again! See github issue #191.
	
	    var generatedMappings = aSourceMap._mappings.toArray().slice();
	    var destGeneratedMappings = smc.__generatedMappings = [];
	    var destOriginalMappings = smc.__originalMappings = [];
	
	    for (var i = 0, length = generatedMappings.length; i < length; i++) {
	      var srcMapping = generatedMappings[i];
	      var destMapping = new Mapping;
	      destMapping.generatedLine = srcMapping.generatedLine;
	      destMapping.generatedColumn = srcMapping.generatedColumn;
	
	      if (srcMapping.source) {
	        destMapping.source = sources.indexOf(srcMapping.source);
	        destMapping.originalLine = srcMapping.originalLine;
	        destMapping.originalColumn = srcMapping.originalColumn;
	
	        if (srcMapping.name) {
	          destMapping.name = names.indexOf(srcMapping.name);
	        }
	
	        destOriginalMappings.push(destMapping);
	      }
	
	      destGeneratedMappings.push(destMapping);
	    }
	
	    quickSort(smc.__originalMappings, util.compareByOriginalPositions);
	
	    return smc;
	  };
	
	/**
	 * The version of the source mapping spec that we are consuming.
	 */
	BasicSourceMapConsumer.prototype._version = 3;
	
	/**
	 * The list of original sources.
	 */
	Object.defineProperty(BasicSourceMapConsumer.prototype, 'sources', {
	  get: function () {
	    return this._absoluteSources.slice();
	  }
	});
	
	/**
	 * Provide the JIT with a nice shape / hidden class.
	 */
	function Mapping() {
	  this.generatedLine = 0;
	  this.generatedColumn = 0;
	  this.source = null;
	  this.originalLine = null;
	  this.originalColumn = null;
	  this.name = null;
	}
	
	/**
	 * Parse the mappings in a string in to a data structure which we can easily
	 * query (the ordered arrays in the `this.__generatedMappings` and
	 * `this.__originalMappings` properties).
	 */
	BasicSourceMapConsumer.prototype._parseMappings =
	  function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	    var generatedLine = 1;
	    var previousGeneratedColumn = 0;
	    var previousOriginalLine = 0;
	    var previousOriginalColumn = 0;
	    var previousSource = 0;
	    var previousName = 0;
	    var length = aStr.length;
	    var index = 0;
	    var cachedSegments = {};
	    var temp = {};
	    var originalMappings = [];
	    var generatedMappings = [];
	    var mapping, str, segment, end, value;
	
	    while (index < length) {
	      if (aStr.charAt(index) === ';') {
	        generatedLine++;
	        index++;
	        previousGeneratedColumn = 0;
	      }
	      else if (aStr.charAt(index) === ',') {
	        index++;
	      }
	      else {
	        mapping = new Mapping();
	        mapping.generatedLine = generatedLine;
	
	        // Because each offset is encoded relative to the previous one,
	        // many segments often have the same encoding. We can exploit this
	        // fact by caching the parsed variable length fields of each segment,
	        // allowing us to avoid a second parse if we encounter the same
	        // segment again.
	        for (end = index; end < length; end++) {
	          if (this._charIsMappingSeparator(aStr, end)) {
	            break;
	          }
	        }
	        str = aStr.slice(index, end);
	
	        segment = cachedSegments[str];
	        if (segment) {
	          index += str.length;
	        } else {
	          segment = [];
	          while (index < end) {
	            base64VLQ.decode(aStr, index, temp);
	            value = temp.value;
	            index = temp.rest;
	            segment.push(value);
	          }
	
	          if (segment.length === 2) {
	            throw new Error('Found a source, but no line and column');
	          }
	
	          if (segment.length === 3) {
	            throw new Error('Found a source and line, but no column');
	          }
	
	          cachedSegments[str] = segment;
	        }
	
	        // Generated column.
	        mapping.generatedColumn = previousGeneratedColumn + segment[0];
	        previousGeneratedColumn = mapping.generatedColumn;
	
	        if (segment.length > 1) {
	          // Original source.
	          mapping.source = previousSource + segment[1];
	          previousSource += segment[1];
	
	          // Original line.
	          mapping.originalLine = previousOriginalLine + segment[2];
	          previousOriginalLine = mapping.originalLine;
	          // Lines are stored 0-based
	          mapping.originalLine += 1;
	
	          // Original column.
	          mapping.originalColumn = previousOriginalColumn + segment[3];
	          previousOriginalColumn = mapping.originalColumn;
	
	          if (segment.length > 4) {
	            // Original name.
	            mapping.name = previousName + segment[4];
	            previousName += segment[4];
	          }
	        }
	
	        generatedMappings.push(mapping);
	        if (typeof mapping.originalLine === 'number') {
	          originalMappings.push(mapping);
	        }
	      }
	    }
	
	    quickSort(generatedMappings, util.compareByGeneratedPositionsDeflated);
	    this.__generatedMappings = generatedMappings;
	
	    quickSort(originalMappings, util.compareByOriginalPositions);
	    this.__originalMappings = originalMappings;
	  };
	
	/**
	 * Find the mapping that best matches the hypothetical "needle" mapping that
	 * we are searching for in the given "haystack" of mappings.
	 */
	BasicSourceMapConsumer.prototype._findMapping =
	  function SourceMapConsumer_findMapping(aNeedle, aMappings, aLineName,
	                                         aColumnName, aComparator, aBias) {
	    // To return the position we are searching for, we must first find the
	    // mapping for the given position and then return the opposite position it
	    // points to. Because the mappings are sorted, we can use binary search to
	    // find the best mapping.
	
	    if (aNeedle[aLineName] <= 0) {
	      throw new TypeError('Line must be greater than or equal to 1, got '
	                          + aNeedle[aLineName]);
	    }
	    if (aNeedle[aColumnName] < 0) {
	      throw new TypeError('Column must be greater than or equal to 0, got '
	                          + aNeedle[aColumnName]);
	    }
	
	    return binarySearch.search(aNeedle, aMappings, aComparator, aBias);
	  };
	
	/**
	 * Compute the last column for each generated mapping. The last column is
	 * inclusive.
	 */
	BasicSourceMapConsumer.prototype.computeColumnSpans =
	  function SourceMapConsumer_computeColumnSpans() {
	    for (var index = 0; index < this._generatedMappings.length; ++index) {
	      var mapping = this._generatedMappings[index];
	
	      // Mappings do not contain a field for the last generated columnt. We
	      // can come up with an optimistic estimate, however, by assuming that
	      // mappings are contiguous (i.e. given two consecutive mappings, the
	      // first mapping ends where the second one starts).
	      if (index + 1 < this._generatedMappings.length) {
	        var nextMapping = this._generatedMappings[index + 1];
	
	        if (mapping.generatedLine === nextMapping.generatedLine) {
	          mapping.lastGeneratedColumn = nextMapping.generatedColumn - 1;
	          continue;
	        }
	      }
	
	      // The last mapping for each line spans the entire line.
	      mapping.lastGeneratedColumn = Infinity;
	    }
	  };
	
	/**
	 * Returns the original source, line, and column information for the generated
	 * source's line and column positions provided. The only argument is an object
	 * with the following properties:
	 *
	 *   - line: The line number in the generated source.  The line number
	 *     is 1-based.
	 *   - column: The column number in the generated source.  The column
	 *     number is 0-based.
	 *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
	 *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - source: The original source file, or null.
	 *   - line: The line number in the original source, or null.  The
	 *     line number is 1-based.
	 *   - column: The column number in the original source, or null.  The
	 *     column number is 0-based.
	 *   - name: The original identifier, or null.
	 */
	BasicSourceMapConsumer.prototype.originalPositionFor =
	  function SourceMapConsumer_originalPositionFor(aArgs) {
	    var needle = {
	      generatedLine: util.getArg(aArgs, 'line'),
	      generatedColumn: util.getArg(aArgs, 'column')
	    };
	
	    var index = this._findMapping(
	      needle,
	      this._generatedMappings,
	      "generatedLine",
	      "generatedColumn",
	      util.compareByGeneratedPositionsDeflated,
	      util.getArg(aArgs, 'bias', SourceMapConsumer.GREATEST_LOWER_BOUND)
	    );
	
	    if (index >= 0) {
	      var mapping = this._generatedMappings[index];
	
	      if (mapping.generatedLine === needle.generatedLine) {
	        var source = util.getArg(mapping, 'source', null);
	        if (source !== null) {
	          source = this._sources.at(source);
	          source = util.computeSourceURL(this.sourceRoot, source, this._sourceMapURL);
	        }
	        var name = util.getArg(mapping, 'name', null);
	        if (name !== null) {
	          name = this._names.at(name);
	        }
	        return {
	          source: source,
	          line: util.getArg(mapping, 'originalLine', null),
	          column: util.getArg(mapping, 'originalColumn', null),
	          name: name
	        };
	      }
	    }
	
	    return {
	      source: null,
	      line: null,
	      column: null,
	      name: null
	    };
	  };
	
	/**
	 * Return true if we have the source content for every source in the source
	 * map, false otherwise.
	 */
	BasicSourceMapConsumer.prototype.hasContentsOfAllSources =
	  function BasicSourceMapConsumer_hasContentsOfAllSources() {
	    if (!this.sourcesContent) {
	      return false;
	    }
	    return this.sourcesContent.length >= this._sources.size() &&
	      !this.sourcesContent.some(function (sc) { return sc == null; });
	  };
	
	/**
	 * Returns the original source content. The only argument is the url of the
	 * original source file. Returns null if no original source content is
	 * available.
	 */
	BasicSourceMapConsumer.prototype.sourceContentFor =
	  function SourceMapConsumer_sourceContentFor(aSource, nullOnMissing) {
	    if (!this.sourcesContent) {
	      return null;
	    }
	
	    var index = this._findSourceIndex(aSource);
	    if (index >= 0) {
	      return this.sourcesContent[index];
	    }
	
	    var relativeSource = aSource;
	    if (this.sourceRoot != null) {
	      relativeSource = util.relative(this.sourceRoot, relativeSource);
	    }
	
	    var url;
	    if (this.sourceRoot != null
	        && (url = util.urlParse(this.sourceRoot))) {
	      // XXX: file:// URIs and absolute paths lead to unexpected behavior for
	      // many users. We can help them out when they expect file:// URIs to
	      // behave like it would if they were running a local HTTP server. See
	      // https://bugzilla.mozilla.org/show_bug.cgi?id=885597.
	      var fileUriAbsPath = relativeSource.replace(/^file:\/\//, "");
	      if (url.scheme == "file"
	          && this._sources.has(fileUriAbsPath)) {
	        return this.sourcesContent[this._sources.indexOf(fileUriAbsPath)]
	      }
	
	      if ((!url.path || url.path == "/")
	          && this._sources.has("/" + relativeSource)) {
	        return this.sourcesContent[this._sources.indexOf("/" + relativeSource)];
	      }
	    }
	
	    // This function is used recursively from
	    // IndexedSourceMapConsumer.prototype.sourceContentFor. In that case, we
	    // don't want to throw if we can't find the source - we just want to
	    // return null, so we provide a flag to exit gracefully.
	    if (nullOnMissing) {
	      return null;
	    }
	    else {
	      throw new Error('"' + relativeSource + '" is not in the SourceMap.');
	    }
	  };
	
	/**
	 * Returns the generated line and column information for the original source,
	 * line, and column positions provided. The only argument is an object with
	 * the following properties:
	 *
	 *   - source: The filename of the original source.
	 *   - line: The line number in the original source.  The line number
	 *     is 1-based.
	 *   - column: The column number in the original source.  The column
	 *     number is 0-based.
	 *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
	 *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - line: The line number in the generated source, or null.  The
	 *     line number is 1-based.
	 *   - column: The column number in the generated source, or null.
	 *     The column number is 0-based.
	 */
	BasicSourceMapConsumer.prototype.generatedPositionFor =
	  function SourceMapConsumer_generatedPositionFor(aArgs) {
	    var source = util.getArg(aArgs, 'source');
	    source = this._findSourceIndex(source);
	    if (source < 0) {
	      return {
	        line: null,
	        column: null,
	        lastColumn: null
	      };
	    }
	
	    var needle = {
	      source: source,
	      originalLine: util.getArg(aArgs, 'line'),
	      originalColumn: util.getArg(aArgs, 'column')
	    };
	
	    var index = this._findMapping(
	      needle,
	      this._originalMappings,
	      "originalLine",
	      "originalColumn",
	      util.compareByOriginalPositions,
	      util.getArg(aArgs, 'bias', SourceMapConsumer.GREATEST_LOWER_BOUND)
	    );
	
	    if (index >= 0) {
	      var mapping = this._originalMappings[index];
	
	      if (mapping.source === needle.source) {
	        return {
	          line: util.getArg(mapping, 'generatedLine', null),
	          column: util.getArg(mapping, 'generatedColumn', null),
	          lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	        };
	      }
	    }
	
	    return {
	      line: null,
	      column: null,
	      lastColumn: null
	    };
	  };
	
	exports.BasicSourceMapConsumer = BasicSourceMapConsumer;
	
	/**
	 * An IndexedSourceMapConsumer instance represents a parsed source map which
	 * we can query for information. It differs from BasicSourceMapConsumer in
	 * that it takes "indexed" source maps (i.e. ones with a "sections" field) as
	 * input.
	 *
	 * The first parameter is a raw source map (either as a JSON string, or already
	 * parsed to an object). According to the spec for indexed source maps, they
	 * have the following attributes:
	 *
	 *   - version: Which version of the source map spec this map is following.
	 *   - file: Optional. The generated file this source map is associated with.
	 *   - sections: A list of section definitions.
	 *
	 * Each value under the "sections" field has two fields:
	 *   - offset: The offset into the original specified at which this section
	 *       begins to apply, defined as an object with a "line" and "column"
	 *       field.
	 *   - map: A source map definition. This source map could also be indexed,
	 *       but doesn't have to be.
	 *
	 * Instead of the "map" field, it's also possible to have a "url" field
	 * specifying a URL to retrieve a source map from, but that's currently
	 * unsupported.
	 *
	 * Here's an example source map, taken from the source map spec[0], but
	 * modified to omit a section which uses the "url" field.
	 *
	 *  {
	 *    version : 3,
	 *    file: "app.js",
	 *    sections: [{
	 *      offset: {line:100, column:10},
	 *      map: {
	 *        version : 3,
	 *        file: "section.js",
	 *        sources: ["foo.js", "bar.js"],
	 *        names: ["src", "maps", "are", "fun"],
	 *        mappings: "AAAA,E;;ABCDE;"
	 *      }
	 *    }],
	 *  }
	 *
	 * The second parameter, if given, is a string whose value is the URL
	 * at which the source map was found.  This URL is used to compute the
	 * sources array.
	 *
	 * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit#heading=h.535es3xeprgt
	 */
	function IndexedSourceMapConsumer(aSourceMap, aSourceMapURL) {
	  var sourceMap = aSourceMap;
	  if (typeof aSourceMap === 'string') {
	    sourceMap = util.parseSourceMapInput(aSourceMap);
	  }
	
	  var version = util.getArg(sourceMap, 'version');
	  var sections = util.getArg(sourceMap, 'sections');
	
	  if (version != this._version) {
	    throw new Error('Unsupported version: ' + version);
	  }
	
	  this._sources = new ArraySet();
	  this._names = new ArraySet();
	
	  var lastOffset = {
	    line: -1,
	    column: 0
	  };
	  this._sections = sections.map(function (s) {
	    if (s.url) {
	      // The url field will require support for asynchronicity.
	      // See https://github.com/mozilla/source-map/issues/16
	      throw new Error('Support for url field in sections not implemented.');
	    }
	    var offset = util.getArg(s, 'offset');
	    var offsetLine = util.getArg(offset, 'line');
	    var offsetColumn = util.getArg(offset, 'column');
	
	    if (offsetLine < lastOffset.line ||
	        (offsetLine === lastOffset.line && offsetColumn < lastOffset.column)) {
	      throw new Error('Section offsets must be ordered and non-overlapping.');
	    }
	    lastOffset = offset;
	
	    return {
	      generatedOffset: {
	        // The offset fields are 0-based, but we use 1-based indices when
	        // encoding/decoding from VLQ.
	        generatedLine: offsetLine + 1,
	        generatedColumn: offsetColumn + 1
	      },
	      consumer: new SourceMapConsumer(util.getArg(s, 'map'), aSourceMapURL)
	    }
	  });
	}
	
	IndexedSourceMapConsumer.prototype = Object.create(SourceMapConsumer.prototype);
	IndexedSourceMapConsumer.prototype.constructor = SourceMapConsumer;
	
	/**
	 * The version of the source mapping spec that we are consuming.
	 */
	IndexedSourceMapConsumer.prototype._version = 3;
	
	/**
	 * The list of original sources.
	 */
	Object.defineProperty(IndexedSourceMapConsumer.prototype, 'sources', {
	  get: function () {
	    var sources = [];
	    for (var i = 0; i < this._sections.length; i++) {
	      for (var j = 0; j < this._sections[i].consumer.sources.length; j++) {
	        sources.push(this._sections[i].consumer.sources[j]);
	      }
	    }
	    return sources;
	  }
	});
	
	/**
	 * Returns the original source, line, and column information for the generated
	 * source's line and column positions provided. The only argument is an object
	 * with the following properties:
	 *
	 *   - line: The line number in the generated source.  The line number
	 *     is 1-based.
	 *   - column: The column number in the generated source.  The column
	 *     number is 0-based.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - source: The original source file, or null.
	 *   - line: The line number in the original source, or null.  The
	 *     line number is 1-based.
	 *   - column: The column number in the original source, or null.  The
	 *     column number is 0-based.
	 *   - name: The original identifier, or null.
	 */
	IndexedSourceMapConsumer.prototype.originalPositionFor =
	  function IndexedSourceMapConsumer_originalPositionFor(aArgs) {
	    var needle = {
	      generatedLine: util.getArg(aArgs, 'line'),
	      generatedColumn: util.getArg(aArgs, 'column')
	    };
	
	    // Find the section containing the generated position we're trying to map
	    // to an original position.
	    var sectionIndex = binarySearch.search(needle, this._sections,
	      function(needle, section) {
	        var cmp = needle.generatedLine - section.generatedOffset.generatedLine;
	        if (cmp) {
	          return cmp;
	        }
	
	        return (needle.generatedColumn -
	                section.generatedOffset.generatedColumn);
	      });
	    var section = this._sections[sectionIndex];
	
	    if (!section) {
	      return {
	        source: null,
	        line: null,
	        column: null,
	        name: null
	      };
	    }
	
	    return section.consumer.originalPositionFor({
	      line: needle.generatedLine -
	        (section.generatedOffset.generatedLine - 1),
	      column: needle.generatedColumn -
	        (section.generatedOffset.generatedLine === needle.generatedLine
	         ? section.generatedOffset.generatedColumn - 1
	         : 0),
	      bias: aArgs.bias
	    });
	  };
	
	/**
	 * Return true if we have the source content for every source in the source
	 * map, false otherwise.
	 */
	IndexedSourceMapConsumer.prototype.hasContentsOfAllSources =
	  function IndexedSourceMapConsumer_hasContentsOfAllSources() {
	    return this._sections.every(function (s) {
	      return s.consumer.hasContentsOfAllSources();
	    });
	  };
	
	/**
	 * Returns the original source content. The only argument is the url of the
	 * original source file. Returns null if no original source content is
	 * available.
	 */
	IndexedSourceMapConsumer.prototype.sourceContentFor =
	  function IndexedSourceMapConsumer_sourceContentFor(aSource, nullOnMissing) {
	    for (var i = 0; i < this._sections.length; i++) {
	      var section = this._sections[i];
	
	      var content = section.consumer.sourceContentFor(aSource, true);
	      if (content) {
	        return content;
	      }
	    }
	    if (nullOnMissing) {
	      return null;
	    }
	    else {
	      throw new Error('"' + aSource + '" is not in the SourceMap.');
	    }
	  };
	
	/**
	 * Returns the generated line and column information for the original source,
	 * line, and column positions provided. The only argument is an object with
	 * the following properties:
	 *
	 *   - source: The filename of the original source.
	 *   - line: The line number in the original source.  The line number
	 *     is 1-based.
	 *   - column: The column number in the original source.  The column
	 *     number is 0-based.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - line: The line number in the generated source, or null.  The
	 *     line number is 1-based. 
	 *   - column: The column number in the generated source, or null.
	 *     The column number is 0-based.
	 */
	IndexedSourceMapConsumer.prototype.generatedPositionFor =
	  function IndexedSourceMapConsumer_generatedPositionFor(aArgs) {
	    for (var i = 0; i < this._sections.length; i++) {
	      var section = this._sections[i];
	
	      // Only consider this section if the requested source is in the list of
	      // sources of the consumer.
	      if (section.consumer._findSourceIndex(util.getArg(aArgs, 'source')) === -1) {
	        continue;
	      }
	      var generatedPosition = section.consumer.generatedPositionFor(aArgs);
	      if (generatedPosition) {
	        var ret = {
	          line: generatedPosition.line +
	            (section.generatedOffset.generatedLine - 1),
	          column: generatedPosition.column +
	            (section.generatedOffset.generatedLine === generatedPosition.line
	             ? section.generatedOffset.generatedColumn - 1
	             : 0)
	        };
	        return ret;
	      }
	    }
	
	    return {
	      line: null,
	      column: null
	    };
	  };
	
	/**
	 * Parse the mappings in a string in to a data structure which we can easily
	 * query (the ordered arrays in the `this.__generatedMappings` and
	 * `this.__originalMappings` properties).
	 */
	IndexedSourceMapConsumer.prototype._parseMappings =
	  function IndexedSourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	    this.__generatedMappings = [];
	    this.__originalMappings = [];
	    for (var i = 0; i < this._sections.length; i++) {
	      var section = this._sections[i];
	      var sectionMappings = section.consumer._generatedMappings;
	      for (var j = 0; j < sectionMappings.length; j++) {
	        var mapping = sectionMappings[j];
	
	        var source = section.consumer._sources.at(mapping.source);
	        source = util.computeSourceURL(section.consumer.sourceRoot, source, this._sourceMapURL);
	        this._sources.add(source);
	        source = this._sources.indexOf(source);
	
	        var name = null;
	        if (mapping.name) {
	          name = section.consumer._names.at(mapping.name);
	          this._names.add(name);
	          name = this._names.indexOf(name);
	        }
	
	        // The mappings coming from the consumer for the section have
	        // generated positions relative to the start of the section, so we
	        // need to offset them to be relative to the start of the concatenated
	        // generated file.
	        var adjustedMapping = {
	          source: source,
	          generatedLine: mapping.generatedLine +
	            (section.generatedOffset.generatedLine - 1),
	          generatedColumn: mapping.generatedColumn +
	            (section.generatedOffset.generatedLine === mapping.generatedLine
	            ? section.generatedOffset.generatedColumn - 1
	            : 0),
	          originalLine: mapping.originalLine,
	          originalColumn: mapping.originalColumn,
	          name: name
	        };
	
	        this.__generatedMappings.push(adjustedMapping);
	        if (typeof adjustedMapping.originalLine === 'number') {
	          this.__originalMappings.push(adjustedMapping);
	        }
	      }
	    }
	
	    quickSort(this.__generatedMappings, util.compareByGeneratedPositionsDeflated);
	    quickSort(this.__originalMappings, util.compareByOriginalPositions);
	  };
	
	exports.IndexedSourceMapConsumer = IndexedSourceMapConsumer;


/***/ }),
/* 4 */
/***/ (function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	exports.GREATEST_LOWER_BOUND = 1;
	exports.LEAST_UPPER_BOUND = 2;
	
	/**
	 * Recursive implementation of binary search.
	 *
	 * @param aLow Indices here and lower do not contain the needle.
	 * @param aHigh Indices here and higher do not contain the needle.
	 * @param aNeedle The element being searched for.
	 * @param aHaystack The non-empty array being searched.
	 * @param aCompare Function which takes two elements and returns -1, 0, or 1.
	 * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
	 *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 */
	function recursiveSearch(aLow, aHigh, aNeedle, aHaystack, aCompare, aBias) {
	  // This function terminates when one of the following is true:
	  //
	  //   1. We find the exact element we are looking for.
	  //
	  //   2. We did not find the exact element, but we can return the index of
	  //      the next-closest element.
	  //
	  //   3. We did not find the exact element, and there is no next-closest
	  //      element than the one we are searching for, so we return -1.
	  var mid = Math.floor((aHigh - aLow) / 2) + aLow;
	  var cmp = aCompare(aNeedle, aHaystack[mid], true);
	  if (cmp === 0) {
	    // Found the element we are looking for.
	    return mid;
	  }
	  else if (cmp > 0) {
	    // Our needle is greater than aHaystack[mid].
	    if (aHigh - mid > 1) {
	      // The element is in the upper half.
	      return recursiveSearch(mid, aHigh, aNeedle, aHaystack, aCompare, aBias);
	    }
	
	    // The exact needle element was not found in this haystack. Determine if
	    // we are in termination case (3) or (2) and return the appropriate thing.
	    if (aBias == exports.LEAST_UPPER_BOUND) {
	      return aHigh < aHaystack.length ? aHigh : -1;
	    } else {
	      return mid;
	    }
	  }
	  else {
	    // Our needle is less than aHaystack[mid].
	    if (mid - aLow > 1) {
	      // The element is in the lower half.
	      return recursiveSearch(aLow, mid, aNeedle, aHaystack, aCompare, aBias);
	    }
	
	    // we are in termination case (3) or (2) and return the appropriate thing.
	    if (aBias == exports.LEAST_UPPER_BOUND) {
	      return mid;
	    } else {
	      return aLow < 0 ? -1 : aLow;
	    }
	  }
	}
	
	/**
	 * This is an implementation of binary search which will always try and return
	 * the index of the closest element if there is no exact hit. This is because
	 * mappings between original and generated line/col pairs are single points,
	 * and there is an implicit region between each of them, so a miss just means
	 * that you aren't on the very start of a region.
	 *
	 * @param aNeedle The element you are looking for.
	 * @param aHaystack The array that is being searched.
	 * @param aCompare A function which takes the needle and an element in the
	 *     array and returns -1, 0, or 1 depending on whether the needle is less
	 *     than, equal to, or greater than the element, respectively.
	 * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
	 *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 *     Defaults to 'binarySearch.GREATEST_LOWER_BOUND'.
	 */
	exports.search = function search(aNeedle, aHaystack, aCompare, aBias) {
	  if (aHaystack.length === 0) {
	    return -1;
	  }
	
	  var index = recursiveSearch(-1, aHaystack.length, aNeedle, aHaystack,
	                              aCompare, aBias || exports.GREATEST_LOWER_BOUND);
	  if (index < 0) {
	    return -1;
	  }
	
	  // We have found either the exact element, or the next-closest element than
	  // the one we are searching for. However, there may be more than one such
	  // element. Make sure we always return the smallest of these.
	  while (index - 1 >= 0) {
	    if (aCompare(aHaystack[index], aHaystack[index - 1], true) !== 0) {
	      break;
	    }
	    --index;
	  }
	
	  return index;
	};


/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(2);
	var has = Object.prototype.hasOwnProperty;
	var hasNativeMap = typeof Map !== "undefined";
	
	/**
	 * A data structure which is a combination of an array and a set. Adding a new
	 * member is O(1), testing for membership is O(1), and finding the index of an
	 * element is O(1). Removing elements from the set is not supported. Only
	 * strings are supported for membership.
	 */
	function ArraySet() {
	  this._array = [];
	  this._set = hasNativeMap ? new Map() : Object.create(null);
	}
	
	/**
	 * Static method for creating ArraySet instances from an existing array.
	 */
	ArraySet.fromArray = function ArraySet_fromArray(aArray, aAllowDuplicates) {
	  var set = new ArraySet();
	  for (var i = 0, len = aArray.length; i < len; i++) {
	    set.add(aArray[i], aAllowDuplicates);
	  }
	  return set;
	};
	
	/**
	 * Return how many unique items are in this ArraySet. If duplicates have been
	 * added, than those do not count towards the size.
	 *
	 * @returns Number
	 */
	ArraySet.prototype.size = function ArraySet_size() {
	  return hasNativeMap ? this._set.size : Object.getOwnPropertyNames(this._set).length;
	};
	
	/**
	 * Add the given string to this set.
	 *
	 * @param String aStr
	 */
	ArraySet.prototype.add = function ArraySet_add(aStr, aAllowDuplicates) {
	  var sStr = hasNativeMap ? aStr : util.toSetString(aStr);
	  var isDuplicate = hasNativeMap ? this.has(aStr) : has.call(this._set, sStr);
	  var idx = this._array.length;
	  if (!isDuplicate || aAllowDuplicates) {
	    this._array.push(aStr);
	  }
	  if (!isDuplicate) {
	    if (hasNativeMap) {
	      this._set.set(aStr, idx);
	    } else {
	      this._set[sStr] = idx;
	    }
	  }
	};
	
	/**
	 * Is the given string a member of this set?
	 *
	 * @param String aStr
	 */
	ArraySet.prototype.has = function ArraySet_has(aStr) {
	  if (hasNativeMap) {
	    return this._set.has(aStr);
	  } else {
	    var sStr = util.toSetString(aStr);
	    return has.call(this._set, sStr);
	  }
	};
	
	/**
	 * What is the index of the given string in the array?
	 *
	 * @param String aStr
	 */
	ArraySet.prototype.indexOf = function ArraySet_indexOf(aStr) {
	  if (hasNativeMap) {
	    var idx = this._set.get(aStr);
	    if (idx >= 0) {
	        return idx;
	    }
	  } else {
	    var sStr = util.toSetString(aStr);
	    if (has.call(this._set, sStr)) {
	      return this._set[sStr];
	    }
	  }
	
	  throw new Error('"' + aStr + '" is not in the set.');
	};
	
	/**
	 * What is the element at the given index?
	 *
	 * @param Number aIdx
	 */
	ArraySet.prototype.at = function ArraySet_at(aIdx) {
	  if (aIdx >= 0 && aIdx < this._array.length) {
	    return this._array[aIdx];
	  }
	  throw new Error('No element indexed by ' + aIdx);
	};
	
	/**
	 * Returns the array representation of this set (which has the proper indices
	 * indicated by indexOf). Note that this is a copy of the internal array used
	 * for storing the members so that no one can mess with internal state.
	 */
	ArraySet.prototype.toArray = function ArraySet_toArray() {
	  return this._array.slice();
	};
	
	exports.ArraySet = ArraySet;


/***/ }),
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 *
	 * Based on the Base 64 VLQ implementation in Closure Compiler:
	 * https://code.google.com/p/closure-compiler/source/browse/trunk/src/com/google/debugging/sourcemap/Base64VLQ.java
	 *
	 * Copyright 2011 The Closure Compiler Authors. All rights reserved.
	 * Redistribution and use in source and binary forms, with or without
	 * modification, are permitted provided that the following conditions are
	 * met:
	 *
	 *  * Redistributions of source code must retain the above copyright
	 *    notice, this list of conditions and the following disclaimer.
	 *  * Redistributions in binary form must reproduce the above
	 *    copyright notice, this list of conditions and the following
	 *    disclaimer in the documentation and/or other materials provided
	 *    with the distribution.
	 *  * Neither the name of Google Inc. nor the names of its
	 *    contributors may be used to endorse or promote products derived
	 *    from this software without specific prior written permission.
	 *
	 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	 */
	
	var base64 = __webpack_require__(7);
	
	// A single base 64 digit can contain 6 bits of data. For the base 64 variable
	// length quantities we use in the source map spec, the first bit is the sign,
	// the next four bits are the actual value, and the 6th bit is the
	// continuation bit. The continuation bit tells us whether there are more
	// digits in this value following this digit.
	//
	//   Continuation
	//   |    Sign
	//   |    |
	//   V    V
	//   101011
	
	var VLQ_BASE_SHIFT = 5;
	
	// binary: 100000
	var VLQ_BASE = 1 << VLQ_BASE_SHIFT;
	
	// binary: 011111
	var VLQ_BASE_MASK = VLQ_BASE - 1;
	
	// binary: 100000
	var VLQ_CONTINUATION_BIT = VLQ_BASE;
	
	/**
	 * Converts from a two-complement value to a value where the sign bit is
	 * placed in the least significant bit.  For example, as decimals:
	 *   1 becomes 2 (10 binary), -1 becomes 3 (11 binary)
	 *   2 becomes 4 (100 binary), -2 becomes 5 (101 binary)
	 */
	function toVLQSigned(aValue) {
	  return aValue < 0
	    ? ((-aValue) << 1) + 1
	    : (aValue << 1) + 0;
	}
	
	/**
	 * Converts to a two-complement value from a value where the sign bit is
	 * placed in the least significant bit.  For example, as decimals:
	 *   2 (10 binary) becomes 1, 3 (11 binary) becomes -1
	 *   4 (100 binary) becomes 2, 5 (101 binary) becomes -2
	 */
	function fromVLQSigned(aValue) {
	  var isNegative = (aValue & 1) === 1;
	  var shifted = aValue >> 1;
	  return isNegative
	    ? -shifted
	    : shifted;
	}
	
	/**
	 * Returns the base 64 VLQ encoded value.
	 */
	exports.encode = function base64VLQ_encode(aValue) {
	  var encoded = "";
	  var digit;
	
	  var vlq = toVLQSigned(aValue);
	
	  do {
	    digit = vlq & VLQ_BASE_MASK;
	    vlq >>>= VLQ_BASE_SHIFT;
	    if (vlq > 0) {
	      // There are still more digits in this value, so we must make sure the
	      // continuation bit is marked.
	      digit |= VLQ_CONTINUATION_BIT;
	    }
	    encoded += base64.encode(digit);
	  } while (vlq > 0);
	
	  return encoded;
	};
	
	/**
	 * Decodes the next base 64 VLQ value from the given string and returns the
	 * value and the rest of the string via the out parameter.
	 */
	exports.decode = function base64VLQ_decode(aStr, aIndex, aOutParam) {
	  var strLen = aStr.length;
	  var result = 0;
	  var shift = 0;
	  var continuation, digit;
	
	  do {
	    if (aIndex >= strLen) {
	      throw new Error("Expected more digits in base 64 VLQ value.");
	    }
	
	    digit = base64.decode(aStr.charCodeAt(aIndex++));
	    if (digit === -1) {
	      throw new Error("Invalid base64 digit: " + aStr.charAt(aIndex - 1));
	    }
	
	    continuation = !!(digit & VLQ_CONTINUATION_BIT);
	    digit &= VLQ_BASE_MASK;
	    result = result + (digit << shift);
	    shift += VLQ_BASE_SHIFT;
	  } while (continuation);
	
	  aOutParam.value = fromVLQSigned(result);
	  aOutParam.rest = aIndex;
	};


/***/ }),
/* 7 */
/***/ (function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var intToCharMap = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'.split('');
	
	/**
	 * Encode an integer in the range of 0 to 63 to a single base 64 digit.
	 */
	exports.encode = function (number) {
	  if (0 <= number && number < intToCharMap.length) {
	    return intToCharMap[number];
	  }
	  throw new TypeError("Must be between 0 and 63: " + number);
	};
	
	/**
	 * Decode a single base 64 character code digit to an integer. Returns -1 on
	 * failure.
	 */
	exports.decode = function (charCode) {
	  var bigA = 65;     // 'A'
	  var bigZ = 90;     // 'Z'
	
	  var littleA = 97;  // 'a'
	  var littleZ = 122; // 'z'
	
	  var zero = 48;     // '0'
	  var nine = 57;     // '9'
	
	  var plus = 43;     // '+'
	  var slash = 47;    // '/'
	
	  var littleOffset = 26;
	  var numberOffset = 52;
	
	  // 0 - 25: ABCDEFGHIJKLMNOPQRSTUVWXYZ
	  if (bigA <= charCode && charCode <= bigZ) {
	    return (charCode - bigA);
	  }
	
	  // 26 - 51: abcdefghijklmnopqrstuvwxyz
	  if (littleA <= charCode && charCode <= littleZ) {
	    return (charCode - littleA + littleOffset);
	  }
	
	  // 52 - 61: 0123456789
	  if (zero <= charCode && charCode <= nine) {
	    return (charCode - zero + numberOffset);
	  }
	
	  // 62: +
	  if (charCode == plus) {
	    return 62;
	  }
	
	  // 63: /
	  if (charCode == slash) {
	    return 63;
	  }
	
	  // Invalid base64 digit.
	  return -1;
	};


/***/ }),
/* 8 */
/***/ (function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	// It turns out that some (most?) JavaScript engines don't self-host
	// `Array.prototype.sort`. This makes sense because C++ will likely remain
	// faster than JS when doing raw CPU-intensive sorting. However, when using a
	// custom comparator function, calling back and forth between the VM's C++ and
	// JIT'd JS is rather slow *and* loses JIT type information, resulting in
	// worse generated code for the comparator function than would be optimal. In
	// fact, when sorting with a comparator, these costs outweigh the benefits of
	// sorting in C++. By using our own JS-implemented Quick Sort (below), we get
	// a ~3500ms mean speed-up in `bench/bench.html`.
	
	/**
	 * Swap the elements indexed by `x` and `y` in the array `ary`.
	 *
	 * @param {Array} ary
	 *        The array.
	 * @param {Number} x
	 *        The index of the first item.
	 * @param {Number} y
	 *        The index of the second item.
	 */
	function swap(ary, x, y) {
	  var temp = ary[x];
	  ary[x] = ary[y];
	  ary[y] = temp;
	}
	
	/**
	 * Returns a random integer within the range `low .. high` inclusive.
	 *
	 * @param {Number} low
	 *        The lower bound on the range.
	 * @param {Number} high
	 *        The upper bound on the range.
	 */
	function randomIntInRange(low, high) {
	  return Math.round(low + (Math.random() * (high - low)));
	}
	
	/**
	 * The Quick Sort algorithm.
	 *
	 * @param {Array} ary
	 *        An array to sort.
	 * @param {function} comparator
	 *        Function to use to compare two items.
	 * @param {Number} p
	 *        Start index of the array
	 * @param {Number} r
	 *        End index of the array
	 */
	function doQuickSort(ary, comparator, p, r) {
	  // If our lower bound is less than our upper bound, we (1) partition the
	  // array into two pieces and (2) recurse on each half. If it is not, this is
	  // the empty array and our base case.
	
	  if (p < r) {
	    // (1) Partitioning.
	    //
	    // The partitioning chooses a pivot between `p` and `r` and moves all
	    // elements that are less than or equal to the pivot to the before it, and
	    // all the elements that are greater than it after it. The effect is that
	    // once partition is done, the pivot is in the exact place it will be when
	    // the array is put in sorted order, and it will not need to be moved
	    // again. This runs in O(n) time.
	
	    // Always choose a random pivot so that an input array which is reverse
	    // sorted does not cause O(n^2) running time.
	    var pivotIndex = randomIntInRange(p, r);
	    var i = p - 1;
	
	    swap(ary, pivotIndex, r);
	    var pivot = ary[r];
	
	    // Immediately after `j` is incremented in this loop, the following hold
	    // true:
	    //
	    //   * Every element in `ary[p .. i]` is less than or equal to the pivot.
	    //
	    //   * Every element in `ary[i+1 .. j-1]` is greater than the pivot.
	    for (var j = p; j < r; j++) {
	      if (comparator(ary[j], pivot) <= 0) {
	        i += 1;
	        swap(ary, i, j);
	      }
	    }
	
	    swap(ary, i + 1, j);
	    var q = i + 1;
	
	    // (2) Recurse on each half.
	
	    doQuickSort(ary, comparator, p, q - 1);
	    doQuickSort(ary, comparator, q + 1, r);
	  }
	}
	
	/**
	 * Sort the given array in-place with the given comparator function.
	 *
	 * @param {Array} ary
	 *        An array to sort.
	 * @param {function} comparator
	 *        Function to use to compare two items.
	 */
	exports.quickSort = function (ary, comparator) {
	  doQuickSort(ary, comparator, 0, ary.length - 1);
	};


/***/ }),
/* 9 */
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var base64VLQ = __webpack_require__(6);
	var util = __webpack_require__(2);
	var ArraySet = __webpack_require__(5).ArraySet;
	var MappingList = __webpack_require__(10).MappingList;
	
	/**
	 * An instance of the SourceMapGenerator represents a source map which is
	 * being built incrementally. You may pass an object with the following
	 * properties:
	 *
	 *   - file: The filename of the generated source.
	 *   - sourceRoot: A root for all relative URLs in this source map.
	 */
	function SourceMapGenerator(aArgs) {
	  if (!aArgs) {
	    aArgs = {};
	  }
	  this._file = util.getArg(aArgs, 'file', null);
	  this._sourceRoot = util.getArg(aArgs, 'sourceRoot', null);
	  this._skipValidation = util.getArg(aArgs, 'skipValidation', false);
	  this._sources = new ArraySet();
	  this._names = new ArraySet();
	  this._mappings = new MappingList();
	  this._sourcesContents = null;
	}
	
	SourceMapGenerator.prototype._version = 3;
	
	/**
	 * Creates a new SourceMapGenerator based on a SourceMapConsumer
	 *
	 * @param aSourceMapConsumer The SourceMap.
	 */
	SourceMapGenerator.fromSourceMap =
	  function SourceMapGenerator_fromSourceMap(aSourceMapConsumer) {
	    var sourceRoot = aSourceMapConsumer.sourceRoot;
	    var generator = new SourceMapGenerator({
	      file: aSourceMapConsumer.file,
	      sourceRoot: sourceRoot
	    });
	    aSourceMapConsumer.eachMapping(function (mapping) {
	      var newMapping = {
	        generated: {
	          line: mapping.generatedLine,
	          column: mapping.generatedColumn
	        }
	      };
	
	      if (mapping.source != null) {
	        newMapping.source = mapping.source;
	        if (sourceRoot != null) {
	          newMapping.source = util.relative(sourceRoot, newMapping.source);
	        }
	
	        newMapping.original = {
	          line: mapping.originalLine,
	          column: mapping.originalColumn
	        };
	
	        if (mapping.name != null) {
	          newMapping.name = mapping.name;
	        }
	      }
	
	      generator.addMapping(newMapping);
	    });
	    aSourceMapConsumer.sources.forEach(function (sourceFile) {
	      var sourceRelative = sourceFile;
	      if (sourceRoot !== null) {
	        sourceRelative = util.relative(sourceRoot, sourceFile);
	      }
	
	      if (!generator._sources.has(sourceRelative)) {
	        generator._sources.add(sourceRelative);
	      }
	
	      var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	      if (content != null) {
	        generator.setSourceContent(sourceFile, content);
	      }
	    });
	    return generator;
	  };
	
	/**
	 * Add a single mapping from original source line and column to the generated
	 * source's line and column for this source map being created. The mapping
	 * object should have the following properties:
	 *
	 *   - generated: An object with the generated line and column positions.
	 *   - original: An object with the original line and column positions.
	 *   - source: The original source file (relative to the sourceRoot).
	 *   - name: An optional original token name for this mapping.
	 */
	SourceMapGenerator.prototype.addMapping =
	  function SourceMapGenerator_addMapping(aArgs) {
	    var generated = util.getArg(aArgs, 'generated');
	    var original = util.getArg(aArgs, 'original', null);
	    var source = util.getArg(aArgs, 'source', null);
	    var name = util.getArg(aArgs, 'name', null);
	
	    if (!this._skipValidation) {
	      this._validateMapping(generated, original, source, name);
	    }
	
	    if (source != null) {
	      source = String(source);
	      if (!this._sources.has(source)) {
	        this._sources.add(source);
	      }
	    }
	
	    if (name != null) {
	      name = String(name);
	      if (!this._names.has(name)) {
	        this._names.add(name);
	      }
	    }
	
	    this._mappings.add({
	      generatedLine: generated.line,
	      generatedColumn: generated.column,
	      originalLine: original != null && original.line,
	      originalColumn: original != null && original.column,
	      source: source,
	      name: name
	    });
	  };
	
	/**
	 * Set the source content for a source file.
	 */
	SourceMapGenerator.prototype.setSourceContent =
	  function SourceMapGenerator_setSourceContent(aSourceFile, aSourceContent) {
	    var source = aSourceFile;
	    if (this._sourceRoot != null) {
	      source = util.relative(this._sourceRoot, source);
	    }
	
	    if (aSourceContent != null) {
	      // Add the source content to the _sourcesContents map.
	      // Create a new _sourcesContents map if the property is null.
	      if (!this._sourcesContents) {
	        this._sourcesContents = Object.create(null);
	      }
	      this._sourcesContents[util.toSetString(source)] = aSourceContent;
	    } else if (this._sourcesContents) {
	      // Remove the source file from the _sourcesContents map.
	      // If the _sourcesContents map is empty, set the property to null.
	      delete this._sourcesContents[util.toSetString(source)];
	      if (Object.keys(this._sourcesContents).length === 0) {
	        this._sourcesContents = null;
	      }
	    }
	  };
	
	/**
	 * Applies the mappings of a sub-source-map for a specific source file to the
	 * source map being generated. Each mapping to the supplied source file is
	 * rewritten using the supplied source map. Note: The resolution for the
	 * resulting mappings is the minimium of this map and the supplied map.
	 *
	 * @param aSourceMapConsumer The source map to be applied.
	 * @param aSourceFile Optional. The filename of the source file.
	 *        If omitted, SourceMapConsumer's file property will be used.
	 * @param aSourceMapPath Optional. The dirname of the path to the source map
	 *        to be applied. If relative, it is relative to the SourceMapConsumer.
	 *        This parameter is needed when the two source maps aren't in the same
	 *        directory, and the source map to be applied contains relative source
	 *        paths. If so, those relative source paths need to be rewritten
	 *        relative to the SourceMapGenerator.
	 */
	SourceMapGenerator.prototype.applySourceMap =
	  function SourceMapGenerator_applySourceMap(aSourceMapConsumer, aSourceFile, aSourceMapPath) {
	    var sourceFile = aSourceFile;
	    // If aSourceFile is omitted, we will use the file property of the SourceMap
	    if (aSourceFile == null) {
	      if (aSourceMapConsumer.file == null) {
	        throw new Error(
	          'SourceMapGenerator.prototype.applySourceMap requires either an explicit source file, ' +
	          'or the source map\'s "file" property. Both were omitted.'
	        );
	      }
	      sourceFile = aSourceMapConsumer.file;
	    }
	    var sourceRoot = this._sourceRoot;
	    // Make "sourceFile" relative if an absolute Url is passed.
	    if (sourceRoot != null) {
	      sourceFile = util.relative(sourceRoot, sourceFile);
	    }
	    // Applying the SourceMap can add and remove items from the sources and
	    // the names array.
	    var newSources = new ArraySet();
	    var newNames = new ArraySet();
	
	    // Find mappings for the "sourceFile"
	    this._mappings.unsortedForEach(function (mapping) {
	      if (mapping.source === sourceFile && mapping.originalLine != null) {
	        // Check if it can be mapped by the source map, then update the mapping.
	        var original = aSourceMapConsumer.originalPositionFor({
	          line: mapping.originalLine,
	          column: mapping.originalColumn
	        });
	        if (original.source != null) {
	          // Copy mapping
	          mapping.source = original.source;
	          if (aSourceMapPath != null) {
	            mapping.source = util.join(aSourceMapPath, mapping.source)
	          }
	          if (sourceRoot != null) {
	            mapping.source = util.relative(sourceRoot, mapping.source);
	          }
	          mapping.originalLine = original.line;
	          mapping.originalColumn = original.column;
	          if (original.name != null) {
	            mapping.name = original.name;
	          }
	        }
	      }
	
	      var source = mapping.source;
	      if (source != null && !newSources.has(source)) {
	        newSources.add(source);
	      }
	
	      var name = mapping.name;
	      if (name != null && !newNames.has(name)) {
	        newNames.add(name);
	      }
	
	    }, this);
	    this._sources = newSources;
	    this._names = newNames;
	
	    // Copy sourcesContents of applied map.
	    aSourceMapConsumer.sources.forEach(function (sourceFile) {
	      var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	      if (content != null) {
	        if (aSourceMapPath != null) {
	          sourceFile = util.join(aSourceMapPath, sourceFile);
	        }
	        if (sourceRoot != null) {
	          sourceFile = util.relative(sourceRoot, sourceFile);
	        }
	        this.setSourceContent(sourceFile, content);
	      }
	    }, this);
	  };
	
	/**
	 * A mapping can have one of the three levels of data:
	 *
	 *   1. Just the generated position.
	 *   2. The Generated position, original position, and original source.
	 *   3. Generated and original position, original source, as well as a name
	 *      token.
	 *
	 * To maintain consistency, we validate that any new mapping being added falls
	 * in to one of these categories.
	 */
	SourceMapGenerator.prototype._validateMapping =
	  function SourceMapGenerator_validateMapping(aGenerated, aOriginal, aSource,
	                                              aName) {
	    // When aOriginal is truthy but has empty values for .line and .column,
	    // it is most likely a programmer error. In this case we throw a very
	    // specific error message to try to guide them the right way.
	    // For example: https://github.com/Polymer/polymer-bundler/pull/519
	    if (aOriginal && typeof aOriginal.line !== 'number' && typeof aOriginal.column !== 'number') {
	        throw new Error(
	            'original.line and original.column are not numbers -- you probably meant to omit ' +
	            'the original mapping entirely and only map the generated position. If so, pass ' +
	            'null for the original mapping instead of an object with empty or null values.'
	        );
	    }
	
	    if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
	        && aGenerated.line > 0 && aGenerated.column >= 0
	        && !aOriginal && !aSource && !aName) {
	      // Case 1.
	      return;
	    }
	    else if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
	             && aOriginal && 'line' in aOriginal && 'column' in aOriginal
	             && aGenerated.line > 0 && aGenerated.column >= 0
	             && aOriginal.line > 0 && aOriginal.column >= 0
	             && aSource) {
	      // Cases 2 and 3.
	      return;
	    }
	    else {
	      throw new Error('Invalid mapping: ' + JSON.stringify({
	        generated: aGenerated,
	        source: aSource,
	        original: aOriginal,
	        name: aName
	      }));
	    }
	  };
	
	/**
	 * Serialize the accumulated mappings in to the stream of base 64 VLQs
	 * specified by the source map format.
	 */
	SourceMapGenerator.prototype._serializeMappings =
	  function SourceMapGenerator_serializeMappings() {
	    var previousGeneratedColumn = 0;
	    var previousGeneratedLine = 1;
	    var previousOriginalColumn = 0;
	    var previousOriginalLine = 0;
	    var previousName = 0;
	    var previousSource = 0;
	    var result = '';
	    var next;
	    var mapping;
	    var nameIdx;
	    var sourceIdx;
	
	    var mappings = this._mappings.toArray();
	    for (var i = 0, len = mappings.length; i < len; i++) {
	      mapping = mappings[i];
	      next = ''
	
	      if (mapping.generatedLine !== previousGeneratedLine) {
	        previousGeneratedColumn = 0;
	        while (mapping.generatedLine !== previousGeneratedLine) {
	          next += ';';
	          previousGeneratedLine++;
	        }
	      }
	      else {
	        if (i > 0) {
	          if (!util.compareByGeneratedPositionsInflated(mapping, mappings[i - 1])) {
	            continue;
	          }
	          next += ',';
	        }
	      }
	
	      next += base64VLQ.encode(mapping.generatedColumn
	                                 - previousGeneratedColumn);
	      previousGeneratedColumn = mapping.generatedColumn;
	
	      if (mapping.source != null) {
	        sourceIdx = this._sources.indexOf(mapping.source);
	        next += base64VLQ.encode(sourceIdx - previousSource);
	        previousSource = sourceIdx;
	
	        // lines are stored 0-based in SourceMap spec version 3
	        next += base64VLQ.encode(mapping.originalLine - 1
	                                   - previousOriginalLine);
	        previousOriginalLine = mapping.originalLine - 1;
	
	        next += base64VLQ.encode(mapping.originalColumn
	                                   - previousOriginalColumn);
	        previousOriginalColumn = mapping.originalColumn;
	
	        if (mapping.name != null) {
	          nameIdx = this._names.indexOf(mapping.name);
	          next += base64VLQ.encode(nameIdx - previousName);
	          previousName = nameIdx;
	        }
	      }
	
	      result += next;
	    }
	
	    return result;
	  };
	
	SourceMapGenerator.prototype._generateSourcesContent =
	  function SourceMapGenerator_generateSourcesContent(aSources, aSourceRoot) {
	    return aSources.map(function (source) {
	      if (!this._sourcesContents) {
	        return null;
	      }
	      if (aSourceRoot != null) {
	        source = util.relative(aSourceRoot, source);
	      }
	      var key = util.toSetString(source);
	      return Object.prototype.hasOwnProperty.call(this._sourcesContents, key)
	        ? this._sourcesContents[key]
	        : null;
	    }, this);
	  };
	
	/**
	 * Externalize the source map.
	 */
	SourceMapGenerator.prototype.toJSON =
	  function SourceMapGenerator_toJSON() {
	    var map = {
	      version: this._version,
	      sources: this._sources.toArray(),
	      names: this._names.toArray(),
	      mappings: this._serializeMappings()
	    };
	    if (this._file != null) {
	      map.file = this._file;
	    }
	    if (this._sourceRoot != null) {
	      map.sourceRoot = this._sourceRoot;
	    }
	    if (this._sourcesContents) {
	      map.sourcesContent = this._generateSourcesContent(map.sources, map.sourceRoot);
	    }
	
	    return map;
	  };
	
	/**
	 * Render the source map being generated to a string.
	 */
	SourceMapGenerator.prototype.toString =
	  function SourceMapGenerator_toString() {
	    return JSON.stringify(this.toJSON());
	  };
	
	exports.SourceMapGenerator = SourceMapGenerator;


/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2014 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(2);
	
	/**
	 * Determine whether mappingB is after mappingA with respect to generated
	 * position.
	 */
	function generatedPositionAfter(mappingA, mappingB) {
	  // Optimized for most common case
	  var lineA = mappingA.generatedLine;
	  var lineB = mappingB.generatedLine;
	  var columnA = mappingA.generatedColumn;
	  var columnB = mappingB.generatedColumn;
	  return lineB > lineA || lineB == lineA && columnB >= columnA ||
	         util.compareByGeneratedPositionsInflated(mappingA, mappingB) <= 0;
	}
	
	/**
	 * A data structure to provide a sorted view of accumulated mappings in a
	 * performance conscious manner. It trades a neglibable overhead in general
	 * case for a large speedup in case of mappings being added in order.
	 */
	function MappingList() {
	  this._array = [];
	  this._sorted = true;
	  // Serves as infimum
	  this._last = {generatedLine: -1, generatedColumn: 0};
	}
	
	/**
	 * Iterate through internal items. This method takes the same arguments that
	 * `Array.prototype.forEach` takes.
	 *
	 * NOTE: The order of the mappings is NOT guaranteed.
	 */
	MappingList.prototype.unsortedForEach =
	  function MappingList_forEach(aCallback, aThisArg) {
	    this._array.forEach(aCallback, aThisArg);
	  };
	
	/**
	 * Add the given source mapping.
	 *
	 * @param Object aMapping
	 */
	MappingList.prototype.add = function MappingList_add(aMapping) {
	  if (generatedPositionAfter(this._last, aMapping)) {
	    this._last = aMapping;
	    this._array.push(aMapping);
	  } else {
	    this._sorted = false;
	    this._array.push(aMapping);
	  }
	};
	
	/**
	 * Returns the flat, sorted array of mappings. The mappings are sorted by
	 * generated position.
	 *
	 * WARNING: This method returns internal data without copying, for
	 * performance. The return value must NOT be mutated, and should be treated as
	 * an immutable borrow. If you want to take ownership, you must make your own
	 * copy.
	 */
	MappingList.prototype.toArray = function MappingList_toArray() {
	  if (!this._sorted) {
	    this._array.sort(util.compareByGeneratedPositionsInflated);
	    this._sorted = true;
	  }
	  return this._array;
	};
	
	exports.MappingList = MappingList;


/***/ })
/******/ ]);
//# sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgZjlkMDZlY2IxNmIzOTZhNzNlNGUiLCJ3ZWJwYWNrOi8vLy4vdGVzdC90ZXN0LXNvdXJjZS1tYXAtY29uc3VtZXIuanMiLCJ3ZWJwYWNrOi8vLy4vdGVzdC91dGlsLmpzIiwid2VicGFjazovLy8uL2xpYi91dGlsLmpzIiwid2VicGFjazovLy8uL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyLmpzIiwid2VicGFjazovLy8uL2xpYi9iaW5hcnktc2VhcmNoLmpzIiwid2VicGFjazovLy8uL2xpYi9hcnJheS1zZXQuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL2Jhc2U2NC12bHEuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL2Jhc2U2NC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvcXVpY2stc29ydC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvc291cmNlLW1hcC1nZW5lcmF0b3IuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL21hcHBpbmctbGlzdC5qcyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiOzs7Ozs7Ozs7OztBQUFBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLHVCQUFlO0FBQ2Y7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7OztBQUdBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7Ozs7Ozs7QUN0Q0EsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsSUFBRztBQUNIO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7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QTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSwrQ0FBOEMsUUFBUTtBQUN0RDtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLEVBQUM7O0FBRUQ7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUEsNEJBQTJCLFFBQVE7QUFDbkM7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSxjQUFhO0FBQ2I7O0FBRUE7QUFDQSxlQUFjO0FBQ2Q7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSx1Q0FBc0M7QUFDdEM7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7Ozs7OztBQ3ZlQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLEVBQUM7O0FBRUQ7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsRUFBQzs7QUFFRDtBQUNBO0FBQ0E7QUFDQSxvQkFBbUI7QUFDbkI7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsWUFBVzs7QUFFWDtBQUNBO0FBQ0EsUUFBTztBQUNQOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxZQUFXOztBQUVYO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSw0QkFBMkIsTUFBTTtBQUNqQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSxJQUFHOztBQUVIO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsY0FBYSxrQ0FBa0M7QUFDL0M7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUEsdURBQXNELFlBQVk7QUFDbEU7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsRUFBQzs7QUFFRDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSxvQ0FBbUM7QUFDbkM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLDBCQUF5QixjQUFjO0FBQ3ZDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSx3QkFBdUIsd0NBQXdDO0FBQy9EOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsZ0RBQStDLG1CQUFtQixFQUFFO0FBQ3BFOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGtCQUFpQixvQkFBb0I7QUFDckM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLDhCQUE2QixNQUFNO0FBQ25DO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBLElBQUc7QUFDSDs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLG9CQUFtQiwyQkFBMkI7QUFDOUMsc0JBQXFCLCtDQUErQztBQUNwRTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsRUFBQzs7QUFFRDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxvQkFBbUIsMkJBQTJCO0FBQzlDOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esb0JBQW1CLDJCQUEyQjtBQUM5Qzs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxvQkFBbUIsMkJBQTJCO0FBQzlDO0FBQ0E7QUFDQSxzQkFBcUIsNEJBQTRCO0FBQ2pEOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBOzs7Ozs7O0FDeG5DQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7Ozs7OztBQzlHQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSx1Q0FBc0MsU0FBUztBQUMvQztBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLElBQUc7QUFDSDtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOzs7Ozs7O0FDeEhBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLDREQUEyRDtBQUMzRCxxQkFBb0I7QUFDcEI7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FB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OztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFVBQVM7QUFDVDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQSxNQUFLO0FBQ0w7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSwyQ0FBMEMsU0FBUztBQUNuRDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLHFCQUFvQjtBQUNwQjtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7Ozs7Ozs7QUN4YUEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGlCQUFnQjtBQUNoQjs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUEiLCJmaWxlIjoidGVzdF9zb3VyY2VfbWFwX2NvbnN1bWVyLmpzIiwic291cmNlc0NvbnRlbnQiOlsiIFx0Ly8gVGhlIG1vZHVsZSBjYWNoZVxuIFx0dmFyIGluc3RhbGxlZE1vZHVsZXMgPSB7fTtcblxuIFx0Ly8gVGhlIHJlcXVpcmUgZnVuY3Rpb25cbiBcdGZ1bmN0aW9uIF9fd2VicGFja19yZXF1aXJlX18obW9kdWxlSWQpIHtcblxuIFx0XHQvLyBDaGVjayBpZiBtb2R1bGUgaXMgaW4gY2FjaGVcbiBcdFx0aWYoaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0pXG4gXHRcdFx0cmV0dXJuIGluc3RhbGxlZE1vZHVsZXNbbW9kdWxlSWRdLmV4cG9ydHM7XG5cbiBcdFx0Ly8gQ3JlYXRlIGEgbmV3IG1vZHVsZSAoYW5kIHB1dCBpdCBpbnRvIHRoZSBjYWNoZSlcbiBcdFx0dmFyIG1vZHVsZSA9IGluc3RhbGxlZE1vZHVsZXNbbW9kdWxlSWRdID0ge1xuIFx0XHRcdGV4cG9ydHM6IHt9LFxuIFx0XHRcdGlkOiBtb2R1bGVJZCxcbiBcdFx0XHRsb2FkZWQ6IGZhbHNlXG4gXHRcdH07XG5cbiBcdFx0Ly8gRXhlY3V0ZSB0aGUgbW9kdWxlIGZ1bmN0aW9uXG4gXHRcdG1vZHVsZXNbbW9kdWxlSWRdLmNhbGwobW9kdWxlLmV4cG9ydHMsIG1vZHVsZSwgbW9kdWxlLmV4cG9ydHMsIF9fd2VicGFja19yZXF1aXJlX18pO1xuXG4gXHRcdC8vIEZsYWcgdGhlIG1vZHVsZSBhcyBsb2FkZWRcbiBcdFx0bW9kdWxlLmxvYWRlZCA9IHRydWU7XG5cbiBcdFx0Ly8gUmV0dXJuIHRoZSBleHBvcnRzIG9mIHRoZSBtb2R1bGVcbiBcdFx0cmV0dXJuIG1vZHVsZS5leHBvcnRzO1xuIFx0fVxuXG5cbiBcdC8vIGV4cG9zZSB0aGUgbW9kdWxlcyBvYmplY3QgKF9fd2VicGFja19tb2R1bGVzX18pXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLm0gPSBtb2R1bGVzO1xuXG4gXHQvLyBleHBvc2UgdGhlIG1vZHVsZSBjYWNoZVxuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5jID0gaW5zdGFsbGVkTW9kdWxlcztcblxuIFx0Ly8gX193ZWJwYWNrX3B1YmxpY19wYXRoX19cbiBcdF9fd2VicGFja19yZXF1aXJlX18ucCA9IFwiXCI7XG5cbiBcdC8vIExvYWQgZW50cnkgbW9kdWxlIGFuZCByZXR1cm4gZXhwb3J0c1xuIFx0cmV0dXJuIF9fd2VicGFja19yZXF1aXJlX18oMCk7XG5cblxuXG4vLyBXRUJQQUNLIEZPT1RFUiAvL1xuLy8gd2VicGFjay9ib290c3RyYXAgZjlkMDZlY2IxNmIzOTZhNzNlNGUiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbnZhciB1dGlsID0gcmVxdWlyZShcIi4vdXRpbFwiKTtcbnZhciBTb3VyY2VNYXBDb25zdW1lciA9IHJlcXVpcmUoJy4uL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyJykuU291cmNlTWFwQ29uc3VtZXI7XG52YXIgSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyID0gcmVxdWlyZSgnLi4vbGliL3NvdXJjZS1tYXAtY29uc3VtZXInKS5JbmRleGVkU291cmNlTWFwQ29uc3VtZXI7XG52YXIgQmFzaWNTb3VyY2VNYXBDb25zdW1lciA9IHJlcXVpcmUoJy4uL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyJykuQmFzaWNTb3VyY2VNYXBDb25zdW1lcjtcbnZhciBTb3VyY2VNYXBHZW5lcmF0b3IgPSByZXF1aXJlKCcuLi9saWIvc291cmNlLW1hcC1nZW5lcmF0b3InKS5Tb3VyY2VNYXBHZW5lcmF0b3I7XG5cbmV4cG9ydHNbJ3Rlc3QgdGhhdCB3ZSBjYW4gaW5zdGFudGlhdGUgd2l0aCBhIHN0cmluZyBvciBhbiBvYmplY3QnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuICB9KTtcbiAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihKU09OLnN0cmluZ2lmeSh1dGlsLnRlc3RNYXApKTtcbiAgfSk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IHRoYXQgdGhlIG9iamVjdCByZXR1cm5lZCBmcm9tIG5ldyBTb3VyY2VNYXBDb25zdW1lciBpbmhlcml0cyBmcm9tIFNvdXJjZU1hcENvbnN1bWVyJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIGFzc2VydC5vayhuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwKSBpbnN0YW5jZW9mIFNvdXJjZU1hcENvbnN1bWVyKTtcbn1cblxuZXhwb3J0c1sndGVzdCB0aGF0IGEgQmFzaWNTb3VyY2VNYXBDb25zdW1lciBpcyByZXR1cm5lZCBmb3Igc291cmNlbWFwcyB3aXRob3V0IHNlY3Rpb25zJ10gPSBmdW5jdGlvbihhc3NlcnQpIHtcbiAgYXNzZXJ0Lm9rKG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApIGluc3RhbmNlb2YgQmFzaWNTb3VyY2VNYXBDb25zdW1lcik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IHRoYXQgYW4gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyIGlzIHJldHVybmVkIGZvciBzb3VyY2VtYXBzIHdpdGggc2VjdGlvbnMnXSA9IGZ1bmN0aW9uKGFzc2VydCkge1xuICBhc3NlcnQub2sobmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuaW5kZXhlZFRlc3RNYXApIGluc3RhbmNlb2YgSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgdGhhdCB0aGUgYHNvdXJjZXNgIGZpZWxkIGhhcyB0aGUgb3JpZ2luYWwgc291cmNlcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwO1xuICB2YXIgc291cmNlcztcblxuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwKTtcbiAgc291cmNlcyA9IG1hcC5zb3VyY2VzO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJy90aGUvcm9vdC9vbmUuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMV0sICcvdGhlL3Jvb3QvdHdvLmpzJyk7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMik7XG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuaW5kZXhlZFRlc3RNYXApO1xuICBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzWzBdLCAnL3RoZS9yb290L29uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1sxXSwgJy90aGUvcm9vdC90d28uanMnKTtcbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXMubGVuZ3RoLCAyKTtcblxuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC5pbmRleGVkVGVzdE1hcERpZmZlcmVudFNvdXJjZVJvb3RzKTtcbiAgc291cmNlcyA9IG1hcC5zb3VyY2VzO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJy90aGUvcm9vdC9vbmUuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMV0sICcvZGlmZmVyZW50L3Jvb3QvdHdvLmpzJyk7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMik7XG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcE5vU291cmNlUm9vdCk7XG4gIHNvdXJjZXMgPSBtYXAuc291cmNlcztcbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMF0sICdvbmUuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMV0sICd0d28uanMnKTtcbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXMubGVuZ3RoLCAyKTtcblxuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwRW1wdHlTb3VyY2VSb290KTtcbiAgc291cmNlcyA9IG1hcC5zb3VyY2VzO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJ29uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1sxXSwgJ3R3by5qcycpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlcy5sZW5ndGgsIDIpO1xufTtcblxuZXhwb3J0c1sndGVzdCB0aGF0IHRoZSBzb3VyY2Ugcm9vdCBpcyByZWZsZWN0ZWQgaW4gYSBtYXBwaW5nXFwncyBzb3VyY2UgZmllbGQnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcDtcbiAgdmFyIG1hcHBpbmc7XG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCk7XG5cbiAgbWFwcGluZyA9IG1hcC5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICBsaW5lOiAyLFxuICAgIGNvbHVtbjogMVxuICB9KTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmcuc291cmNlLCAnL3RoZS9yb290L3R3by5qcycpO1xuXG4gIG1hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgbGluZTogMSxcbiAgICBjb2x1bW46IDFcbiAgfSk7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZSwgJy90aGUvcm9vdC9vbmUuanMnKTtcblxuXG4gIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXBOb1NvdXJjZVJvb3QpO1xuXG4gIG1hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgbGluZTogMixcbiAgICBjb2x1bW46IDFcbiAgfSk7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZSwgJ3R3by5qcycpO1xuXG4gIG1hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgbGluZTogMSxcbiAgICBjb2x1bW46IDFcbiAgfSk7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZSwgJ29uZS5qcycpO1xuXG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcEVtcHR5U291cmNlUm9vdCk7XG5cbiAgbWFwcGluZyA9IG1hcC5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICBsaW5lOiAyLFxuICAgIGNvbHVtbjogMVxuICB9KTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmcuc291cmNlLCAndHdvLmpzJyk7XG5cbiAgbWFwcGluZyA9IG1hcC5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICBsaW5lOiAxLFxuICAgIGNvbHVtbjogMVxuICB9KTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmcuc291cmNlLCAnb25lLmpzJyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IG1hcHBpbmcgdG9rZW5zIGJhY2sgZXhhY3RseSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCk7XG5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDEsICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgNSwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCA1LCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCA5LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDExLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMSwgJ2JhcicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDIxLCAnL3RoZS9yb290L29uZS5qcycsIDIsIDMsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDI4LCAnL3RoZS9yb290L29uZS5qcycsIDIsIDEwLCAnYmF6JywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMzIsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTQsICdiYXInLCBudWxsLCBtYXAsIGFzc2VydCk7XG5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDEsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgNSwgJy90aGUvcm9vdC90d28uanMnLCAxLCA1LCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA5LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDExLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAxOCwgJy90aGUvcm9vdC90d28uanMnLCAxLCAyMSwgJ24nLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAyMSwgJy90aGUvcm9vdC90d28uanMnLCAyLCAzLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAyOCwgJy90aGUvcm9vdC90d28uanMnLCAyLCAxMCwgJ24nLCBudWxsLCBtYXAsIGFzc2VydCk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IG1hcHBpbmcgdG9rZW5zIGJhY2sgZXhhY3RseSBpbiBpbmRleGVkIHNvdXJjZSBtYXAnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcblxuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMSwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAxLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCA1LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDUsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDksICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMTEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE4LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjEsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMywgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTAsICdiYXonLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAzMiwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxNCwgJ2JhcicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcblxuICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMSwgJy90aGUvcm9vdC90d28uanMnLCAxLCAxLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA1LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDUsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDksICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDE4LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDIxLCAnbicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDIxLCAnL3RoZS9yb290L3R3by5qcycsIDIsIDMsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDI4LCAnL3RoZS9yb290L3R3by5qcycsIDIsIDEwLCAnbicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgbWFwcGluZyB0b2tlbnMgZnV6enknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuXG4gIC8vIEZpbmRpbmcgb3JpZ2luYWwgcG9zaXRpb25zIHdpdGggZGVmYXVsdCAoZ2xiKSBiaWFzLlxuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjAsICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMjEsICdiYXInLCBudWxsLCBtYXAsIGFzc2VydCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAzMCwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxMCwgJ2JheicsIG51bGwsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDEyLCAnL3RoZS9yb290L3R3by5qcycsIDEsIDExLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCwgdHJ1ZSk7XG5cbiAgLy8gRmluZGluZyBvcmlnaW5hbCBwb3NpdGlvbnMgd2l0aCBsdWIgYmlhcy5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE2LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDI2LCAnL3RoZS9yb290L29uZS5qcycsIDIsIDEwLCAnYmF6JywgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDYsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTEsIG51bGwsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgdHJ1ZSk7XG5cbiAgLy8gRmluZGluZyBnZW5lcmF0ZWQgcG9zaXRpb25zIHdpdGggZGVmYXVsdCAoZ2xiKSBiaWFzLlxuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMTgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMjIsICdiYXInLCBudWxsLCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAyOCwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxMywgJ2JheicsIG51bGwsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDksICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTYsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcblxuICAvLyBGaW5kaW5nIGdlbmVyYXRlZCBwb3NpdGlvbnMgd2l0aCBsdWIgYmlhcy5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE4LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIwLCAnYmFyJywgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDI4LCAnL3RoZS9yb290L29uZS5qcycsIDIsIDcsICdiYXonLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgOSwgJy90aGUvcm9vdC90d28uanMnLCAxLCA2LCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xufTtcblxuZXhwb3J0c1sndGVzdCBtYXBwaW5nIHRva2VucyBmdXp6eSBpbiBpbmRleGVkIHNvdXJjZSBtYXAnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcblxuICAvLyBGaW5kaW5nIG9yaWdpbmFsIHBvc2l0aW9ucyB3aXRoIGRlZmF1bHQgKGdsYikgYmlhcy5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDIwLCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMzAsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTAsICdiYXonLCBudWxsLCBtYXAsIGFzc2VydCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAxMiwgJy90aGUvcm9vdC90d28uanMnLCAxLCAxMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuXG4gIC8vIEZpbmRpbmcgb3JpZ2luYWwgcG9zaXRpb25zIHdpdGggbHViIGJpYXMuXG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxNiwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMSwgJ2JhcicsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAyNiwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxMCwgJ2JheicsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA2LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDExLCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuXG4gIC8vIEZpbmRpbmcgZ2VuZXJhdGVkIHBvc2l0aW9ucyB3aXRoIGRlZmF1bHQgKGdsYikgYmlhcy5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE4LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIyLCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xuICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTMsICdiYXonLCBudWxsLCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA5LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDE2LCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG5cbiAgLy8gRmluZGluZyBnZW5lcmF0ZWQgcG9zaXRpb25zIHdpdGggbHViIGJpYXMuXG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMCwgJ2JhcicsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAyOCwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCA3LCAnYmF6JywgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDksICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgNiwgbnVsbCwgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgbWFwcGluZ3MgYW5kIGVuZCBvZiBsaW5lcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgc21nID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgZmlsZTogJ2Zvby5qcydcbiAgfSk7XG4gIHNtZy5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgc291cmNlOiAnYmFyLmpzJ1xuICB9KTtcbiAgc21nLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICdiYXIuanMnXG4gIH0pO1xuICBzbWcuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIHNvdXJjZTogJ2Jhei5qcydcbiAgfSk7XG5cbiAgdmFyIG1hcCA9IFNvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAoc21nKTtcblxuICAvLyBXaGVuIGZpbmRpbmcgb3JpZ2luYWwgcG9zaXRpb25zLCBtYXBwaW5ncyBlbmQgYXQgdGhlIGVuZCBvZiB0aGUgbGluZS5cbiAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDEsIG51bGwsIG51bGwsIG51bGwsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0LCB0cnVlKVxuXG4gIC8vIFdoZW4gZmluZGluZyBnZW5lcmF0ZWQgcG9zaXRpb25zLCBtYXBwaW5ncyBkbyBub3QgZW5kIGF0IHRoZSBlbmQgb2YgdGhlIGxpbmUuXG4gIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxLCAnYmFyLmpzJywgMiwgMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xuXG4gIC8vIFdoZW4gZmluZGluZyBnZW5lcmF0ZWQgcG9zaXRpb25zIHdpdGgsIG1hcHBpbmdzIGVuZCBhdCB0aGUgZW5kIG9mIHRoZSBzb3VyY2UuXG4gIHV0aWwuYXNzZXJ0TWFwcGluZyhudWxsLCBudWxsLCAnYmFyLmpzJywgMywgMSwgbnVsbCwgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgY3JlYXRpbmcgc291cmNlIG1hcCBjb25zdW1lcnMgd2l0aCApXX1cXCcgcHJlZml4J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIGFzc2VydC5kb2VzTm90VGhyb3coZnVuY3Rpb24gKCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoXCIpXX0nXFxuXCIgKyBKU09OLnN0cmluZ2lmeSh1dGlsLnRlc3RNYXApKTtcbiAgfSk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGVhY2hNYXBwaW5nJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXA7XG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCk7XG4gIHZhciBwcmV2aW91c0xpbmUgPSAtSW5maW5pdHk7XG4gIHZhciBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgbWFwLmVhY2hNYXBwaW5nKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgYXNzZXJ0Lm9rKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA+PSBwcmV2aW91c0xpbmUpO1xuXG4gICAgYXNzZXJ0Lm9rKG1hcHBpbmcuc291cmNlID09PSAnL3RoZS9yb290L29uZS5qcycgfHwgbWFwcGluZy5zb3VyY2UgPT09ICcvdGhlL3Jvb3QvdHdvLmpzJyk7XG5cbiAgICBpZiAobWFwcGluZy5nZW5lcmF0ZWRMaW5lID09PSBwcmV2aW91c0xpbmUpIHtcbiAgICAgIGFzc2VydC5vayhtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiA+PSBwcmV2aW91c0NvbHVtbik7XG4gICAgICBwcmV2aW91c0NvbHVtbiA9IG1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uO1xuICAgIH1cbiAgICBlbHNlIHtcbiAgICAgIHByZXZpb3VzTGluZSA9IG1hcHBpbmcuZ2VuZXJhdGVkTGluZTtcbiAgICAgIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICAgIH1cbiAgfSk7XG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcE5vU291cmNlUm9vdCk7XG4gIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAobWFwcGluZykge1xuICAgIGFzc2VydC5vayhtYXBwaW5nLnNvdXJjZSA9PT0gJ29uZS5qcycgfHwgbWFwcGluZy5zb3VyY2UgPT09ICd0d28uanMnKTtcbiAgfSk7XG5cbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcEVtcHR5U291cmNlUm9vdCk7XG4gIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAobWFwcGluZykge1xuICAgIGFzc2VydC5vayhtYXBwaW5nLnNvdXJjZSA9PT0gJ29uZS5qcycgfHwgbWFwcGluZy5zb3VyY2UgPT09ICd0d28uanMnKTtcbiAgfSk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGVhY2hNYXBwaW5nIGZvciBpbmRleGVkIHNvdXJjZSBtYXBzJ10gPSBmdW5jdGlvbihhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcbiAgdmFyIHByZXZpb3VzTGluZSA9IC1JbmZpbml0eTtcbiAgdmFyIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICBtYXAuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICBhc3NlcnQub2sobWFwcGluZy5nZW5lcmF0ZWRMaW5lID49IHByZXZpb3VzTGluZSk7XG5cbiAgICBpZiAobWFwcGluZy5zb3VyY2UpIHtcbiAgICAgIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZS5pbmRleE9mKHV0aWwudGVzdE1hcC5zb3VyY2VSb290KSwgMCk7XG4gICAgfVxuXG4gICAgaWYgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA9PT0gcHJldmlvdXNMaW5lKSB7XG4gICAgICBhc3NlcnQub2sobWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4gPj0gcHJldmlvdXNDb2x1bW4pO1xuICAgICAgcHJldmlvdXNDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcbiAgICB9XG4gICAgZWxzZSB7XG4gICAgICBwcmV2aW91c0xpbmUgPSBtYXBwaW5nLmdlbmVyYXRlZExpbmU7XG4gICAgICBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICB9XG4gIH0pO1xufTtcblxuXG5leHBvcnRzWyd0ZXN0IGl0ZXJhdGluZyBvdmVyIG1hcHBpbmdzIGluIGEgZGlmZmVyZW50IG9yZGVyJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwKTtcbiAgdmFyIHByZXZpb3VzTGluZSA9IC1JbmZpbml0eTtcbiAgdmFyIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICB2YXIgcHJldmlvdXNTb3VyY2UgPSBcIlwiO1xuICBtYXAuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICBhc3NlcnQub2sobWFwcGluZy5zb3VyY2UgPj0gcHJldmlvdXNTb3VyY2UpO1xuXG4gICAgaWYgKG1hcHBpbmcuc291cmNlID09PSBwcmV2aW91c1NvdXJjZSkge1xuICAgICAgYXNzZXJ0Lm9rKG1hcHBpbmcub3JpZ2luYWxMaW5lID49IHByZXZpb3VzTGluZSk7XG5cbiAgICAgIGlmIChtYXBwaW5nLm9yaWdpbmFsTGluZSA9PT0gcHJldmlvdXNMaW5lKSB7XG4gICAgICAgIGFzc2VydC5vayhtYXBwaW5nLm9yaWdpbmFsQ29sdW1uID49IHByZXZpb3VzQ29sdW1uKTtcbiAgICAgICAgcHJldmlvdXNDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuICAgICAgfVxuICAgICAgZWxzZSB7XG4gICAgICAgIHByZXZpb3VzTGluZSA9IG1hcHBpbmcub3JpZ2luYWxMaW5lO1xuICAgICAgICBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICAgIH1cbiAgICB9XG4gICAgZWxzZSB7XG4gICAgICBwcmV2aW91c1NvdXJjZSA9IG1hcHBpbmcuc291cmNlO1xuICAgICAgcHJldmlvdXNMaW5lID0gLUluZmluaXR5O1xuICAgICAgcHJldmlvdXNDb2x1bW4gPSAtSW5maW5pdHk7XG4gICAgfVxuICB9LCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5PUklHSU5BTF9PUkRFUik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGl0ZXJhdGluZyBvdmVyIG1hcHBpbmdzIGluIGEgZGlmZmVyZW50IG9yZGVyIGluIGluZGV4ZWQgc291cmNlIG1hcHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcbiAgdmFyIHByZXZpb3VzTGluZSA9IC1JbmZpbml0eTtcbiAgdmFyIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICB2YXIgcHJldmlvdXNTb3VyY2UgPSBcIlwiO1xuICBtYXAuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICBhc3NlcnQub2sobWFwcGluZy5zb3VyY2UgPj0gcHJldmlvdXNTb3VyY2UpO1xuXG4gICAgaWYgKG1hcHBpbmcuc291cmNlID09PSBwcmV2aW91c1NvdXJjZSkge1xuICAgICAgYXNzZXJ0Lm9rKG1hcHBpbmcub3JpZ2luYWxMaW5lID49IHByZXZpb3VzTGluZSk7XG5cbiAgICAgIGlmIChtYXBwaW5nLm9yaWdpbmFsTGluZSA9PT0gcHJldmlvdXNMaW5lKSB7XG4gICAgICAgIGFzc2VydC5vayhtYXBwaW5nLm9yaWdpbmFsQ29sdW1uID49IHByZXZpb3VzQ29sdW1uKTtcbiAgICAgICAgcHJldmlvdXNDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuICAgICAgfVxuICAgICAgZWxzZSB7XG4gICAgICAgIHByZXZpb3VzTGluZSA9IG1hcHBpbmcub3JpZ2luYWxMaW5lO1xuICAgICAgICBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICAgIH1cbiAgICB9XG4gICAgZWxzZSB7XG4gICAgICBwcmV2aW91c1NvdXJjZSA9IG1hcHBpbmcuc291cmNlO1xuICAgICAgcHJldmlvdXNMaW5lID0gLUluZmluaXR5O1xuICAgICAgcHJldmlvdXNDb2x1bW4gPSAtSW5maW5pdHk7XG4gICAgfVxuICB9LCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5PUklHSU5BTF9PUkRFUik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IHRoYXQgd2UgY2FuIHNldCB0aGUgY29udGV4dCBmb3IgYHRoaXNgIGluIGVhY2hNYXBwaW5nJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwKTtcbiAgdmFyIGNvbnRleHQgPSB7fTtcbiAgbWFwLmVhY2hNYXBwaW5nKGZ1bmN0aW9uICgpIHtcbiAgICBhc3NlcnQuZXF1YWwodGhpcywgY29udGV4dCk7XG4gIH0sIGNvbnRleHQpO1xufTtcblxuZXhwb3J0c1sndGVzdCB0aGF0IHdlIGNhbiBzZXQgdGhlIGNvbnRleHQgZm9yIGB0aGlzYCBpbiBlYWNoTWFwcGluZyBpbiBpbmRleGVkIHNvdXJjZSBtYXBzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC5pbmRleGVkVGVzdE1hcCk7XG4gIHZhciBjb250ZXh0ID0ge307XG4gIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAoKSB7XG4gICAgYXNzZXJ0LmVxdWFsKHRoaXMsIGNvbnRleHQpO1xuICB9LCBjb250ZXh0KTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgdGhhdCB0aGUgYHNvdXJjZXNDb250ZW50YCBmaWVsZCBoYXMgdGhlIG9yaWdpbmFsIHNvdXJjZXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXBXaXRoU291cmNlc0NvbnRlbnQpO1xuICB2YXIgc291cmNlc0NvbnRlbnQgPSBtYXAuc291cmNlc0NvbnRlbnQ7XG5cbiAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNDb250ZW50WzBdLCAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuICAgcmV0dXJuIGJheihiYXIpO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc0NvbnRlbnRbMV0sICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuICAgcmV0dXJuIG4gKyAxO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc0NvbnRlbnQubGVuZ3RoLCAyKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgdGhhdCB3ZSBjYW4gZ2V0IHRoZSBvcmlnaW5hbCBzb3VyY2VzIGZvciB0aGUgc291cmNlcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcFdpdGhTb3VyY2VzQ29udGVudCk7XG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZXNbMF0pLCAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuICAgcmV0dXJuIGJheihiYXIpO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlc1sxXSksICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuICAgcmV0dXJuIG4gKyAxO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJvbmUuanNcIiksICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4gICByZXR1cm4gYmF6KGJhcik7XFxuIH07Jyk7XG4gIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihcInR3by5qc1wiKSwgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4gICByZXR1cm4gbiArIDE7XFxuIH07Jyk7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiXCIpO1xuICB9LCBFcnJvcik7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiL3RoZS9yb290L3RocmVlLmpzXCIpO1xuICB9LCBFcnJvcik7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwidGhyZWUuanNcIik7XG4gIH0sIEVycm9yKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgdGhhdCB3ZSBjYW4gZ2V0IHRoZSBvcmlnaW5hbCBzb3VyY2UgY29udGVudCB3aXRoIHJlbGF0aXZlIHNvdXJjZSBwYXRocyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcFJlbGF0aXZlU291cmNlcyk7XG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZXNbMF0pLCAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuICAgcmV0dXJuIGJheihiYXIpO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlc1sxXSksICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuICAgcmV0dXJuIG4gKyAxO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJvbmUuanNcIiksICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4gICByZXR1cm4gYmF6KGJhcik7XFxuIH07Jyk7XG4gIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihcInR3by5qc1wiKSwgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4gICByZXR1cm4gbiArIDE7XFxuIH07Jyk7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiXCIpO1xuICB9LCBFcnJvcik7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiL3RoZS9yb290L3RocmVlLmpzXCIpO1xuICB9LCBFcnJvcik7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwidGhyZWUuanNcIik7XG4gIH0sIEVycm9yKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgdGhhdCB3ZSBjYW4gZ2V0IHRoZSBvcmlnaW5hbCBzb3VyY2UgY29udGVudCBmb3IgdGhlIHNvdXJjZXMgb24gYW4gaW5kZXhlZCBzb3VyY2UgbWFwJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC5pbmRleGVkVGVzdE1hcCk7XG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZXNbMF0pLCAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuICAgcmV0dXJuIGJheihiYXIpO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlc1sxXSksICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuICAgcmV0dXJuIG4gKyAxO1xcbiB9OycpO1xuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJvbmUuanNcIiksICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4gICByZXR1cm4gYmF6KGJhcik7XFxuIH07Jyk7XG4gIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihcInR3by5qc1wiKSwgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4gICByZXR1cm4gbiArIDE7XFxuIH07Jyk7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiXCIpO1xuICB9LCBFcnJvcik7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiL3RoZS9yb290L3RocmVlLmpzXCIpO1xuICB9LCBFcnJvcik7XG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwidGhyZWUuanNcIik7XG4gIH0sIEVycm9yKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgaGFzQ29udGVudHNPZkFsbFNvdXJjZXMsIHNpbmdsZSBzb3VyY2Ugd2l0aCBjb250ZW50cyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAvLyBIYXMgb25lIHNvdXJjZTogZm9vLmpzICh3aXRoIGNvbnRlbnRzKS5cbiAgdmFyIG1hcFdpdGhDb250ZW50cyA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3IoKTtcbiAgbWFwV2l0aENvbnRlbnRzLmFkZE1hcHBpbmcoeyBzb3VyY2U6ICdmb28uanMnLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAxMCB9IH0pO1xuICBtYXBXaXRoQ29udGVudHMuc2V0U291cmNlQ29udGVudCgnZm9vLmpzJywgJ2NvbnRlbnQgb2YgZm9vLmpzJyk7XG4gIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXBXaXRoQ29udGVudHMudG9KU09OKCkpO1xuICBhc3NlcnQub2soY29uc3VtZXIuaGFzQ29udGVudHNPZkFsbFNvdXJjZXMoKSk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzLCBzaW5nbGUgc291cmNlIHdpdGhvdXQgY29udGVudHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgLy8gSGFzIG9uZSBzb3VyY2U6IGZvby5qcyAod2l0aG91dCBjb250ZW50cykuXG4gIHZhciBtYXBXaXRob3V0Q29udGVudHMgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKCk7XG4gIG1hcFdpdGhvdXRDb250ZW50cy5hZGRNYXBwaW5nKHsgc291cmNlOiAnZm9vLmpzJyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0sXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSB9KTtcbiAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcFdpdGhvdXRDb250ZW50cy50b0pTT04oKSk7XG4gIGFzc2VydC5vayghY29uc3VtZXIuaGFzQ29udGVudHNPZkFsbFNvdXJjZXMoKSk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzLCB0d28gc291cmNlcyB3aXRoIGNvbnRlbnRzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIC8vIEhhcyB0d28gc291cmNlczogZm9vLmpzICh3aXRoIGNvbnRlbnRzKSBhbmQgYmFyLmpzICh3aXRoIGNvbnRlbnRzKS5cbiAgdmFyIG1hcFdpdGhCb3RoQ29udGVudHMgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKCk7XG4gIG1hcFdpdGhCb3RoQ29udGVudHMuYWRkTWFwcGluZyh7IHNvdXJjZTogJ2Zvby5qcycsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSB9KTtcbiAgbWFwV2l0aEJvdGhDb250ZW50cy5hZGRNYXBwaW5nKHsgc291cmNlOiAnYmFyLmpzJyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxMCB9LFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAxMCB9IH0pO1xuICBtYXBXaXRoQm90aENvbnRlbnRzLnNldFNvdXJjZUNvbnRlbnQoJ2Zvby5qcycsICdjb250ZW50IG9mIGZvby5qcycpO1xuICBtYXBXaXRoQm90aENvbnRlbnRzLnNldFNvdXJjZUNvbnRlbnQoJ2Jhci5qcycsICdjb250ZW50IG9mIGJhci5qcycpO1xuICB2YXIgY29uc3VtZXIgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwV2l0aEJvdGhDb250ZW50cy50b0pTT04oKSk7XG4gIGFzc2VydC5vayhjb25zdW1lci5oYXNDb250ZW50c09mQWxsU291cmNlcygpKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgaGFzQ29udGVudHNPZkFsbFNvdXJjZXMsIHR3byBzb3VyY2VzIG9uZSB3aXRoIGFuZCBvbmUgd2l0aG91dCBjb250ZW50cyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAvLyBIYXMgdHdvIHNvdXJjZXM6IGZvby5qcyAod2l0aCBjb250ZW50cykgYW5kIGJhci5qcyAod2l0aG91dCBjb250ZW50cykuXG4gIHZhciBtYXBXaXRob3V0U29tZUNvbnRlbnRzID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcigpO1xuICBtYXBXaXRob3V0U29tZUNvbnRlbnRzLmFkZE1hcHBpbmcoeyBzb3VyY2U6ICdmb28uanMnLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0sXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0gfSk7XG4gIG1hcFdpdGhvdXRTb21lQ29udGVudHMuYWRkTWFwcGluZyh7IHNvdXJjZTogJ2Jhci5qcycsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSB9KTtcbiAgbWFwV2l0aG91dFNvbWVDb250ZW50cy5zZXRTb3VyY2VDb250ZW50KCdmb28uanMnLCAnY29udGVudCBvZiBmb28uanMnKTtcbiAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcFdpdGhvdXRTb21lQ29udGVudHMudG9KU09OKCkpO1xuICBhc3NlcnQub2soIWNvbnN1bWVyLmhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzKCkpO1xufTtcblxuZXhwb3J0c1sndGVzdCBzb3VyY2VSb290ICsgZ2VuZXJhdGVkUG9zaXRpb25Gb3InXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgIHNvdXJjZVJvb3Q6ICdmb28vYmFyJyxcbiAgICBmaWxlOiAnYmF6LmpzJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICdiYW5nLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiA1LCBjb2x1bW46IDUgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogNiwgY29sdW1uOiA2IH0sXG4gICAgc291cmNlOiAnYmFuZy5jb2ZmZWUnXG4gIH0pO1xuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCksICdodHRwOi8vZXhhbXBsZS5jb20vJyk7XG5cbiAgLy8gU2hvdWxkIGhhbmRsZSB3aXRob3V0IHNvdXJjZVJvb3QuXG4gIHZhciBwb3MgPSBtYXAuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDEsXG4gICAgY29sdW1uOiAxLFxuICAgIHNvdXJjZTogJ2JhbmcuY29mZmVlJ1xuICB9KTtcblxuICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDIpO1xuICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMik7XG5cbiAgLy8gU2hvdWxkIGhhbmRsZSB3aXRoIHNvdXJjZVJvb3QuXG4gIHZhciBwb3MgPSBtYXAuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDEsXG4gICAgY29sdW1uOiAxLFxuICAgIHNvdXJjZTogJ2Zvby9iYXIvYmFuZy5jb2ZmZWUnXG4gIH0pO1xuXG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMik7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAyKTtcblxuICAvLyBTaG91bGQgaGFuZGxlIGFic29sdXRlIGNhc2UuXG4gIHZhciBwb3MgPSBtYXAuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDEsXG4gICAgY29sdW1uOiAxLFxuICAgIHNvdXJjZTogJ2h0dHA6Ly9leGFtcGxlLmNvbS9mb28vYmFyL2JhbmcuY29mZmVlJ1xuICB9KTtcblxuICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDIpO1xuICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IHNvdXJjZVJvb3QgKyBnZW5lcmF0ZWRQb3NpdGlvbkZvciBmb3IgcGF0aCBhYm92ZSB0aGUgcm9vdCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgc291cmNlUm9vdDogJ2Zvby9iYXInLFxuICAgIGZpbGU6ICdiYXouanMnXG4gIH0pO1xuICBtYXAuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgIHNvdXJjZTogJy4uL2JhbmcuY29mZmVlJ1xuICB9KTtcbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICAvLyBTaG91bGQgaGFuZGxlIHdpdGggc291cmNlUm9vdC5cbiAgdmFyIHBvcyA9IG1hcC5nZW5lcmF0ZWRQb3NpdGlvbkZvcih7XG4gICAgbGluZTogMSxcbiAgICBjb2x1bW46IDEsXG4gICAgc291cmNlOiAnZm9vL2JhbmcuY29mZmVlJ1xuICB9KTtcblxuICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDIpO1xuICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGFsbEdlbmVyYXRlZFBvc2l0aW9uc0ZvciBmb3IgbGluZSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgZmlsZTogJ2dlbmVyYXRlZC5qcydcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAyLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMywgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMywgY29sdW1uOiAzIH0sXG4gICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAzLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogNCwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgfSk7XG4gIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS8nKTtcblxuICB2YXIgbWFwcGluZ3MgPSBtYXAuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKHtcbiAgICBsaW5lOiAyLFxuICAgIHNvdXJjZTogJ2Jhci5jb2ZmZWUnXG4gIH0pO1xuXG4gIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDIpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0ubGluZSwgMyk7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5jb2x1bW4sIDIpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0ubGluZSwgMyk7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1sxXS5jb2x1bW4sIDMpO1xuXG4gIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgbGluZTogMixcbiAgICBzb3VyY2U6ICdodHRwOi8vZXhhbXBsZS5jb20vYmFyLmNvZmZlZSdcbiAgfSk7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMik7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5saW5lLCAzKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmNvbHVtbiwgMik7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1sxXS5saW5lLCAzKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmNvbHVtbiwgMyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGFsbEdlbmVyYXRlZFBvc2l0aW9uc0ZvciBmb3IgbGluZSBmdXp6eSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgZmlsZTogJ2dlbmVyYXRlZC5qcydcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAzLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogNCwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgfSk7XG4gIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgbGluZTogMixcbiAgICBzb3VyY2U6ICdiYXIuY29mZmVlJ1xuICB9KTtcblxuICBhc3NlcnQuZXF1YWwobWFwcGluZ3MubGVuZ3RoLCAxKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmxpbmUsIDQpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0uY29sdW1uLCAyKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yIGZvciBlbXB0eSBzb3VyY2UgbWFwJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICBmaWxlOiAnZ2VuZXJhdGVkLmpzJ1xuICB9KTtcbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICB2YXIgbWFwcGluZ3MgPSBtYXAuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKHtcbiAgICBsaW5lOiAyLFxuICAgIHNvdXJjZTogJ2Jhci5jb2ZmZWUnXG4gIH0pO1xuXG4gIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDApO1xufTtcblxuZXhwb3J0c1sndGVzdCBhbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IgZm9yIGNvbHVtbiddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgZmlsZTogJ2dlbmVyYXRlZC5qcydcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAzIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgbGluZTogMSxcbiAgICBjb2x1bW46IDEsXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMik7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5saW5lLCAxKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmNvbHVtbiwgMik7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1sxXS5saW5lLCAxKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmNvbHVtbiwgMyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGFsbEdlbmVyYXRlZFBvc2l0aW9uc0ZvciBmb3IgY29sdW1uIGZ1enp5J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICBmaWxlOiAnZ2VuZXJhdGVkLmpzJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDMgfSxcbiAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICB9KTtcbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICB2YXIgbWFwcGluZ3MgPSBtYXAuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKHtcbiAgICBsaW5lOiAxLFxuICAgIGNvbHVtbjogMCxcbiAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICB9KTtcblxuICBhc3NlcnQuZXF1YWwobWFwcGluZ3MubGVuZ3RoLCAyKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmxpbmUsIDEpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0uY29sdW1uLCAyKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmxpbmUsIDEpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0uY29sdW1uLCAzKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yIGZvciBjb2x1bW4gb24gZGlmZmVyZW50IGxpbmUgZnV6enknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgIGZpbGU6ICdnZW5lcmF0ZWQuanMnXG4gIH0pO1xuICBtYXAuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAxIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gIH0pO1xuICBtYXAuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAxIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMyB9LFxuICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gIH0pO1xuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gIHZhciBtYXBwaW5ncyA9IG1hcC5hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3Ioe1xuICAgIGxpbmU6IDEsXG4gICAgY29sdW1uOiAwLFxuICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gIH0pO1xuXG4gIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDApO1xufTtcblxuZXhwb3J0c1sndGVzdCBjb21wdXRlQ29sdW1uU3BhbnMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgIGZpbGU6ICdnZW5lcmF0ZWQuanMnXG4gIH0pO1xuICBtYXAuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gIH0pO1xuICBtYXAuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAxIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMSB9LFxuICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gIH0pO1xuICBtYXAuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMTAgfSxcbiAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDIsIGNvbHVtbjogMyB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIwIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAzLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMywgY29sdW1uOiAxIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAzLCBjb2x1bW46IDIgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMywgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG4gIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgbWFwLmNvbXB1dGVDb2x1bW5TcGFucygpO1xuXG4gIHZhciBtYXBwaW5ncyA9IG1hcC5hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3Ioe1xuICAgIGxpbmU6IDEsXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMSk7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5sYXN0Q29sdW1uLCBJbmZpbml0eSk7XG5cbiAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgbGluZTogMixcbiAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICB9KTtcblxuICBhc3NlcnQuZXF1YWwobWFwcGluZ3MubGVuZ3RoLCAzKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmxhc3RDb2x1bW4sIDkpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0ubGFzdENvbHVtbiwgMTkpO1xuICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMl0ubGFzdENvbHVtbiwgSW5maW5pdHkpO1xuXG4gIHZhciBtYXBwaW5ncyA9IG1hcC5hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3Ioe1xuICAgIGxpbmU6IDMsXG4gICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgfSk7XG5cbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMik7XG4gIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5sYXN0Q29sdW1uLCAxKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmxhc3RDb2x1bW4sIEluZmluaXR5KTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3Qgc291cmNlUm9vdCArIG9yaWdpbmFsUG9zaXRpb25Gb3InXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgIHNvdXJjZVJvb3Q6ICdmb28vYmFyJyxcbiAgICBmaWxlOiAnYmF6LmpzJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICdiYW5nLmNvZmZlZSdcbiAgfSk7XG4gIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgdmFyIHBvcyA9IG1hcC5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICBsaW5lOiAyLFxuICAgIGNvbHVtbjogMixcbiAgfSk7XG5cbiAgLy8gU2hvdWxkIGFsd2F5cyBoYXZlIHRoZSBwcmVwZW5kZWQgc291cmNlIHJvb3RcbiAgYXNzZXJ0LmVxdWFsKHBvcy5zb3VyY2UsICdmb28vYmFyL2JhbmcuY29mZmVlJyk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMSk7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAxKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgZ2l0aHViIGlzc3VlICM1NiddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgc291cmNlUm9vdDogJ2h0dHA6Ly8nLFxuICAgIGZpbGU6ICd3d3cuZXhhbXBsZS5jb20vZm9vLmpzJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICd3d3cuZXhhbXBsZS5jb20vb3JpZ2luYWwuanMnXG4gIH0pO1xuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMSk7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzWzBdLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbS9vcmlnaW5hbC5qcycpO1xufTtcblxuLy8gV2FzIGdpdGh1YiBpc3N1ZSAjNDMsIGJ1dCB0aGF0J3Mgbm8gbG9uZ2VyIHZhbGlkLlxuZXhwb3J0c1sndGVzdCBzb3VyY2UgcmVzb2x1dGlvbiB3aXRoIHNvdXJjZU1hcFVSTCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgc291cmNlUm9vdDogJycsXG4gICAgZmlsZTogJ2Zvby5qcydcbiAgfSk7XG4gIG1hcC5hZGRNYXBwaW5nKHtcbiAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgc291cmNlOiAnb3JpZ2luYWwuanMnLFxuICB9KTtcbiAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpLCAnaHR0cDovL2Nkbi5leGFtcGxlLmNvbScpO1xuXG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMSxcbiAgICAgICAgICAgICAgICdTaG91bGQgb25seSBiZSBvbmUgc291cmNlLicpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJ2h0dHA6Ly9jZG4uZXhhbXBsZS5jb20vb3JpZ2luYWwuanMnLFxuICAgICAgICAgICAgICAgJ1Nob3VsZCBiZSBqb2luZWQgd2l0aCB0aGUgc291cmNlIG1hcCBVUkwuJyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IHNvdXJjZVJvb3QgcHJlcGVuZGluZyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgc291cmNlUm9vdDogJ2h0dHA6Ly9leGFtcGxlLmNvbS9mb28vYmFyJyxcbiAgICBmaWxlOiAnZm9vLmpzJ1xuICB9KTtcbiAgbWFwLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICcvb3JpZ2luYWwuanMnXG4gIH0pO1xuICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMSxcbiAgICAgICAgICAgICAgICdTaG91bGQgb25seSBiZSBvbmUgc291cmNlLicpO1xuICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9mb28vYmFyL29yaWdpbmFsLmpzJyxcbiAgICAgICAgICAgICAgICdTb3VyY2UgaW5jbHVkZSB0aGUgc291cmNlIHJvb3QuJyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGluZGV4ZWQgc291cmNlIG1hcCBlcnJvcnMgd2hlbiBzZWN0aW9ucyBhcmUgb3V0IG9mIG9yZGVyIGJ5IGxpbmUnXSA9IGZ1bmN0aW9uKGFzc2VydCkge1xuICAvLyBNYWtlIGEgZGVlcCBjb3B5IG9mIHRoZSBpbmRleGVkVGVzdE1hcFxuICB2YXIgbWlzb3JkZXJlZEluZGV4ZWRUZXN0TWFwID0gSlNPTi5wYXJzZShKU09OLnN0cmluZ2lmeSh1dGlsLmluZGV4ZWRUZXN0TWFwKSk7XG5cbiAgbWlzb3JkZXJlZEluZGV4ZWRUZXN0TWFwLnNlY3Rpb25zWzBdLm9mZnNldCA9IHtcbiAgICBsaW5lOiAyLFxuICAgIGNvbHVtbjogMFxuICB9O1xuXG4gIGFzc2VydC50aHJvd3MoZnVuY3Rpb24oKSB7XG4gICAgbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1pc29yZGVyZWRJbmRleGVkVGVzdE1hcCk7XG4gIH0sIEVycm9yKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgZ2l0aHViIGlzc3VlICM2NCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHtcbiAgICBcInZlcnNpb25cIjogMyxcbiAgICBcImZpbGVcIjogXCJmb28uanNcIixcbiAgICBcInNvdXJjZVJvb3RcIjogXCJodHRwOi8vZXhhbXBsZS5jb20vXCIsXG4gICAgXCJzb3VyY2VzXCI6IFtcIi9hXCJdLFxuICAgIFwibmFtZXNcIjogW10sXG4gICAgXCJtYXBwaW5nc1wiOiBcIkFBQ0FcIixcbiAgICBcInNvdXJjZXNDb250ZW50XCI6IFtcImZvb1wiXVxuICB9KTtcblxuICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJhXCIpLCBcImZvb1wiKTtcbiAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiL2FcIiksIFwiZm9vXCIpO1xufTtcblxuZXhwb3J0c1sndGVzdCBmdWxsIHNvdXJjZSBjb250ZW50IHdpdGggc291cmNlTWFwVVJMJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoe1xuICAgICd2ZXJzaW9uJzogMyxcbiAgICAnZmlsZSc6ICdmb28uanMnLFxuICAgICdzb3VyY2VSb290JzogJycsXG4gICAgJ3NvdXJjZXMnOiBbJ29yaWdpbmFsLmpzJ10sXG4gICAgJ25hbWVzJzogW10sXG4gICAgJ21hcHBpbmdzJzogJ0FBQ0EnLFxuICAgICdzb3VyY2VzQ29udGVudCc6IFsneWVsbG93IHdhcmJsZXInXVxuICB9LCAnaHR0cDovL2Nkbi5leGFtcGxlLmNvbScpO1xuXG4gIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcignaHR0cDovL2Nkbi5leGFtcGxlLmNvbS9vcmlnaW5hbC5qcycpLCAneWVsbG93IHdhcmJsZXInLFxuICAgICAgICAgICAgICAgJ1NvdXJjZSBjb250ZW50IHNob3VsZCBiZSBmb3VuZCB1c2luZyBmdWxsIFVSTCcpO1xufTtcblxuZXhwb3J0c1sndGVzdCBidWcgODg1NTk3J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoe1xuICAgIFwidmVyc2lvblwiOiAzLFxuICAgIFwiZmlsZVwiOiBcImZvby5qc1wiLFxuICAgIFwic291cmNlUm9vdFwiOiBcImZpbGU6Ly8vVXNlcnMvQWxHb3JlL0ludmVudGVkL1RoZS9JbnRlcm5ldC9cIixcbiAgICBcInNvdXJjZXNcIjogW1wiL2FcIl0sXG4gICAgXCJuYW1lc1wiOiBbXSxcbiAgICBcIm1hcHBpbmdzXCI6IFwiQUFDQVwiLFxuICAgIFwic291cmNlc0NvbnRlbnRcIjogW1wiZm9vXCJdXG4gIH0pO1xuXG4gIHZhciBzID0gbWFwLnNvdXJjZXNbMF07XG4gIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihzKSwgXCJmb29cIik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGdpdGh1YiBpc3N1ZSAjNzIsIGR1cGxpY2F0ZSBzb3VyY2VzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoe1xuICAgIFwidmVyc2lvblwiOiAzLFxuICAgIFwiZmlsZVwiOiBcImZvby5qc1wiLFxuICAgIFwic291cmNlc1wiOiBbXCJzb3VyY2UxLmpzXCIsIFwic291cmNlMS5qc1wiLCBcInNvdXJjZTMuanNcIl0sXG4gICAgXCJuYW1lc1wiOiBbXSxcbiAgICBcIm1hcHBpbmdzXCI6IFwiO0VBQUM7O0lBRUU7O01FRUVcIixcbiAgICBcInNvdXJjZVJvb3RcIjogXCJodHRwOi8vZXhhbXBsZS5jb21cIlxuICB9KTtcblxuICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDIsXG4gICAgY29sdW1uOiAyXG4gIH0pO1xuICBhc3NlcnQuZXF1YWwocG9zLnNvdXJjZSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zb3VyY2UxLmpzJyk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMSk7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAxKTtcblxuICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDQsXG4gICAgY29sdW1uOiA0XG4gIH0pO1xuICBhc3NlcnQuZXF1YWwocG9zLnNvdXJjZSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zb3VyY2UxLmpzJyk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMyk7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAzKTtcblxuICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDYsXG4gICAgY29sdW1uOiA2XG4gIH0pO1xuICBhc3NlcnQuZXF1YWwocG9zLnNvdXJjZSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zb3VyY2UzLmpzJyk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgNSk7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCA1KTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgZ2l0aHViIGlzc3VlICM3MiwgZHVwbGljYXRlIG5hbWVzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoe1xuICAgIFwidmVyc2lvblwiOiAzLFxuICAgIFwiZmlsZVwiOiBcImZvby5qc1wiLFxuICAgIFwic291cmNlc1wiOiBbXCJzb3VyY2UuanNcIl0sXG4gICAgXCJuYW1lc1wiOiBbXCJuYW1lMVwiLCBcIm5hbWUxXCIsIFwibmFtZTNcIl0sXG4gICAgXCJtYXBwaW5nc1wiOiBcIjtFQUFDQTs7SUFFRUE7O01BRUVFXCIsXG4gICAgXCJzb3VyY2VSb290XCI6IFwiaHR0cDovL2V4YW1wbGUuY29tXCJcbiAgfSk7XG5cbiAgdmFyIHBvcyA9IG1hcC5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICBsaW5lOiAyLFxuICAgIGNvbHVtbjogMlxuICB9KTtcbiAgYXNzZXJ0LmVxdWFsKHBvcy5uYW1lLCAnbmFtZTEnKTtcbiAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCAxKTtcbiAgYXNzZXJ0LmVxdWFsKHBvcy5jb2x1bW4sIDEpO1xuXG4gIHZhciBwb3MgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgbGluZTogNCxcbiAgICBjb2x1bW46IDRcbiAgfSk7XG4gIGFzc2VydC5lcXVhbChwb3MubmFtZSwgJ25hbWUxJyk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMyk7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAzKTtcblxuICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDYsXG4gICAgY29sdW1uOiA2XG4gIH0pO1xuICBhc3NlcnQuZXF1YWwocG9zLm5hbWUsICduYW1lMycpO1xuICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDUpO1xuICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgNSk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IFNvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIHNtZyA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgIHNvdXJjZVJvb3Q6ICdodHRwOi8vZXhhbXBsZS5jb20vJyxcbiAgICBmaWxlOiAnZm9vLmpzJ1xuICB9KTtcbiAgc21nLmFkZE1hcHBpbmcoe1xuICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICBzb3VyY2U6ICdiYXIuanMnXG4gIH0pO1xuICBzbWcuYWRkTWFwcGluZyh7XG4gICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDQsIGNvbHVtbjogNCB9LFxuICAgIHNvdXJjZTogJ2Jhei5qcycsXG4gICAgbmFtZTogJ2RpcnRNY0dpcnQnXG4gIH0pO1xuICBzbWcuc2V0U291cmNlQ29udGVudCgnYmF6LmpzJywgJ2Jhei5qcyBjb250ZW50Jyk7XG5cbiAgdmFyIHNtYyA9IFNvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAoc21nKTtcbiAgYXNzZXJ0LmVxdWFsKHNtYy5maWxlLCAnZm9vLmpzJyk7XG4gIGFzc2VydC5lcXVhbChzbWMuc291cmNlUm9vdCwgJ2h0dHA6Ly9leGFtcGxlLmNvbS8nKTtcbiAgYXNzZXJ0LmVxdWFsKHNtYy5zb3VyY2VzLmxlbmd0aCwgMik7XG4gIGFzc2VydC5lcXVhbChzbWMuc291cmNlc1swXSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9iYXIuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKHNtYy5zb3VyY2VzWzFdLCAnaHR0cDovL2V4YW1wbGUuY29tL2Jhei5qcycpO1xuICBhc3NlcnQuZXF1YWwoc21jLnNvdXJjZUNvbnRlbnRGb3IoJ2Jhei5qcycpLCAnYmF6LmpzIGNvbnRlbnQnKTtcblxuICB2YXIgcG9zID0gc21jLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDIsXG4gICAgY29sdW1uOiAyXG4gIH0pO1xuICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDEpO1xuICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMSk7XG4gIGFzc2VydC5lcXVhbChwb3Muc291cmNlLCAnaHR0cDovL2V4YW1wbGUuY29tL2Jhci5qcycpO1xuICBhc3NlcnQuZXF1YWwocG9zLm5hbWUsIG51bGwpO1xuXG4gIHBvcyA9IHNtYy5nZW5lcmF0ZWRQb3NpdGlvbkZvcih7XG4gICAgbGluZTogMSxcbiAgICBjb2x1bW46IDEsXG4gICAgc291cmNlOiAnaHR0cDovL2V4YW1wbGUuY29tL2Jhci5qcydcbiAgfSk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMik7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAyKTtcblxuICBwb3MgPSBzbWMub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgbGluZTogNCxcbiAgICBjb2x1bW46IDRcbiAgfSk7XG4gIGFzc2VydC5lcXVhbChwb3MubGluZSwgMik7XG4gIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAyKTtcbiAgYXNzZXJ0LmVxdWFsKHBvcy5zb3VyY2UsICdodHRwOi8vZXhhbXBsZS5jb20vYmF6LmpzJyk7XG4gIGFzc2VydC5lcXVhbChwb3MubmFtZSwgJ2RpcnRNY0dpcnQnKTtcblxuICBwb3MgPSBzbWMuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgIGxpbmU6IDIsXG4gICAgY29sdW1uOiAyLFxuICAgIHNvdXJjZTogJ2h0dHA6Ly9leGFtcGxlLmNvbS9iYXouanMnXG4gIH0pO1xuICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDQpO1xuICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgNCk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGlzc3VlICMxOTEnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIGdlbmVyYXRvciA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3IoeyBmaWxlOiAnYS5jc3MnIH0pO1xuICBnZW5lcmF0b3IuYWRkTWFwcGluZyh7XG4gICAgc291cmNlOiAgICdiLmNzcycsXG4gICAgb3JpZ2luYWw6IHtcbiAgICAgIGxpbmU6ICAgMSxcbiAgICAgIGNvbHVtbjogMFxuICAgIH0sXG4gICAgZ2VuZXJhdGVkOiB7XG4gICAgICBsaW5lOiAgIDEsXG4gICAgICBjb2x1bW46IDBcbiAgICB9XG4gIH0pO1xuXG4gIC8vIENyZWF0ZSBhIFNvdXJjZU1hcENvbnN1bWVyIGZyb20gdGhlIFNvdXJjZU1hcEdlbmVyYXRvciwgLi4uXG4gIHZhciBjb25zdW1lciAgPSBTb3VyY2VNYXBDb25zdW1lci5mcm9tU291cmNlTWFwKGdlbmVyYXRvcik7XG4gIC8vIC4uLiBhbmQgdGhlbiB0cnkgYW5kIHVzZSB0aGUgU291cmNlTWFwR2VuZXJhdG9yIGFnYWluLiBUaGlzIHNob3VsZCBub3RcbiAgLy8gdGhyb3cuXG4gIGdlbmVyYXRvci50b0pTT04oKTtcblxuICBhc3NlcnQub2sodHJ1ZSwgXCJVc2luZyBhIFNvdXJjZU1hcEdlbmVyYXRvciBhZ2FpbiBhZnRlciBjcmVhdGluZyBhIFwiICtcbiAgICAgICAgICAgICAgICAgIFwiU291cmNlTWFwQ29uc3VtZXIgZnJvbSBpdCBzaG91bGQgbm90IHRocm93XCIpO1xufTtcblxuZXhwb3J0c1sndGVzdCBzb3VyY2VzIHdoZXJlIHRoZWlyIHByZWZpeCBpcyB0aGUgc291cmNlIHJvb3Q6IGlzc3VlICMxOTknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIHRlc3RTb3VyY2VNYXAgPSB7XG4gICAgXCJ2ZXJzaW9uXCI6IDMsXG4gICAgXCJzb3VyY2VzXCI6IFtcIi9zb3VyY2UvYXBwL2FwcC9hcHAuanNcIl0sXG4gICAgXCJuYW1lc1wiOiBbXCJTeXN0ZW1cIl0sXG4gICAgXCJtYXBwaW5nc1wiOiBcIkFBQUFBXCIsXG4gICAgXCJmaWxlXCI6IFwiYXBwL2FwcC5qc1wiLFxuICAgIFwic291cmNlc0NvbnRlbnRcIjogW1wiJ3VzZSBzdHJpY3QnO1wiXSxcbiAgICBcInNvdXJjZVJvb3RcIjpcIi9zb3VyY2UvXCJcbiAgfTtcblxuICB2YXIgY29uc3VtZXIgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodGVzdFNvdXJjZU1hcCk7XG5cbiAgZnVuY3Rpb24gY29uc3VtZXJIYXNTb3VyY2Uocykge1xuICAgIGFzc2VydC5vayhjb25zdW1lci5zb3VyY2VDb250ZW50Rm9yKHMpKTtcbiAgfVxuXG4gIGNvbnN1bWVyLnNvdXJjZXMuZm9yRWFjaChjb25zdW1lckhhc1NvdXJjZSk7XG4gIHRlc3RTb3VyY2VNYXAuc291cmNlcy5mb3JFYWNoKGNvbnN1bWVySGFzU291cmNlKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3Qgc291cmNlcyB3aGVyZSB0aGVpciBwcmVmaXggaXMgdGhlIHNvdXJjZSByb290IGFuZCB0aGUgc291cmNlIHJvb3QgaXMgYSB1cmw6IGlzc3VlICMxOTknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIHRlc3RTb3VyY2VNYXAgPSB7XG4gICAgXCJ2ZXJzaW9uXCI6IDMsXG4gICAgXCJzb3VyY2VzXCI6IFtcImh0dHA6Ly9leGFtcGxlLmNvbS9zb3VyY2UvYXBwL2FwcC9hcHAuanNcIl0sXG4gICAgXCJuYW1lc1wiOiBbXCJTeXN0ZW1cIl0sXG4gICAgXCJtYXBwaW5nc1wiOiBcIkFBQUFBXCIsXG4gICAgXCJzb3VyY2VzQ29udGVudFwiOiBbXCIndXNlIHN0cmljdCc7XCJdLFxuICAgIFwic291cmNlUm9vdFwiOlwiaHR0cDovL2V4YW1wbGUuY29tL3NvdXJjZS9cIlxuICB9O1xuXG4gIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih0ZXN0U291cmNlTWFwKTtcblxuICBmdW5jdGlvbiBjb25zdW1lckhhc1NvdXJjZShzKSB7XG4gICAgYXNzZXJ0Lm9rKGNvbnN1bWVyLnNvdXJjZUNvbnRlbnRGb3IocykpO1xuICB9XG5cbiAgY29uc3VtZXIuc291cmNlcy5mb3JFYWNoKGNvbnN1bWVySGFzU291cmNlKTtcbiAgdGVzdFNvdXJjZU1hcC5zb3VyY2VzLmZvckVhY2goY29uc3VtZXJIYXNTb3VyY2UpO1xufTtcblxuZXhwb3J0c1sndGVzdCBjb25zdW1pbmcgbmFtZXMgYW5kIHNvdXJjZXMgdGhhdCBhcmUgbnVtYmVycyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgdGVzdFNvdXJjZU1hcCA9IHtcbiAgICBcInZlcnNpb25cIjogMyxcbiAgICBcInNvdXJjZXNcIjogWzBdLFxuICAgIFwibmFtZXNcIjogWzFdLFxuICAgIFwibWFwcGluZ3NcIjogXCJBQUFBQVwiLFxuICB9O1xuXG4gIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih0ZXN0U291cmNlTWFwKTtcblxuICBhc3NlcnQuZXF1YWwoY29uc3VtZXIuc291cmNlcy5sZW5ndGgsIDEpO1xuICBhc3NlcnQuZXF1YWwoY29uc3VtZXIuc291cmNlc1swXSwgXCIwXCIpO1xuXG4gIHZhciBpID0gMDtcbiAgY29uc3VtZXIuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG0pIHtcbiAgICBpKys7XG4gICAgYXNzZXJ0LmVxdWFsKG0ubmFtZSwgXCIxXCIpO1xuICB9KTtcbiAgYXNzZXJ0LmVxdWFsKGksIDEpO1xufTtcblxuZXhwb3J0c1sndGVzdCBub24tbm9ybWFsaXplZCBzb3VyY2VSb290IChmcm9tIGlzc3VlICMyMjcpJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih7XG4gICAgdmVyc2lvbjogMyxcbiAgICBzb3VyY2VzOiBbICdpbmRleC5qcycgXSxcbiAgICBuYW1lczogW10sXG4gICAgbWFwcGluZ3M6ICc7O0FBQUEsSUFBSSxPQUFPLE1BQVAnLFxuICAgIGZpbGU6ICdpbmRleC5qcycsXG4gICAgc291cmNlUm9vdDogJy4vc3JjLycsXG4gICAgc291cmNlc0NvbnRlbnQ6IFsgJ3ZhciBuYW1lID0gXCJNYXJrXCJcXG4nIF1cbiAgfSk7XG4gIGFzc2VydC5lcXVhbChjb25zdW1lci5zb3VyY2VSb290LCAnc3JjLycsICdzb3VyY2VSb290IHdhcyBub3JtYWxpemVkJyk7XG4gIC8vIEJlZm9yZSB0aGUgZml4LCB0aGlzIHRocmV3IGFuIGV4Y2VwdGlvbi5cbiAgY29uc3VtZXIuc291cmNlQ29udGVudEZvcihjb25zdW1lci5zb3VyY2VzWzBdKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3Qgd2VicGFjayBVUkwgcmVzb2x1dGlvbiddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0ge1xuICAgIHZlcnNpb246IDMsXG4gICAgc291cmNlczogIFtcIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgNjdlMTg0Zjk2Nzk3MzMyOThkNDRcIl0sXG4gICAgbmFtZXM6IFtdLFxuICAgIG1hcHBpbmdzOiBcIkNBQVNcIixcbiAgICBmaWxlOiBcInN0YXRpYy9qcy9tYW5pZmVzdC5iN2NmOTc2ODBmN2E1MGZhMTUwZi5qc1wiLFxuICAgIHNvdXJjZVJvb3Q6IFwiXCJcbiAgfTtcbiAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcCk7XG5cbiAgYXNzZXJ0LmVxdWFsKGNvbnN1bWVyLnNvdXJjZXMubGVuZ3RoLCAxKTtcbiAgYXNzZXJ0LmVxdWFsKGNvbnN1bWVyLnNvdXJjZXNbMF0sIFwid2VicGFjazovLy93ZWJwYWNrL2Jvb3RzdHJhcCA2N2UxODRmOTY3OTczMzI5OGQ0NFwiKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3Qgd2VicGFjayBVUkwgcmVzb2x1dGlvbiB3aXRoIHNvdXJjZU1hcFVSTCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICB2YXIgbWFwID0ge1xuICAgIHZlcnNpb246IDMsXG4gICAgc291cmNlczogIFtcIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgNjdlMTg0Zjk2Nzk3MzMyOThkNDRcIl0sXG4gICAgbmFtZXM6IFtdLFxuICAgIG1hcHBpbmdzOiBcIkNBQVNcIixcbiAgICBmaWxlOiBcInN0YXRpYy9qcy9tYW5pZmVzdC5iN2NmOTc2ODBmN2E1MGZhMTUwZi5qc1wiLFxuICAgIHNvdXJjZVJvb3Q6IFwiXCJcbiAgfTtcbiAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcCwgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20vcS5qcy5tYXAnKTtcblxuICBhc3NlcnQuZXF1YWwoY29uc3VtZXIuc291cmNlcy5sZW5ndGgsIDEpO1xuICBhc3NlcnQuZXF1YWwoY29uc3VtZXIuc291cmNlc1swXSwgXCJ3ZWJwYWNrOi8vL3dlYnBhY2svYm9vdHN0cmFwIDY3ZTE4NGY5Njc5NzMzMjk4ZDQ0XCIpO1xufTtcblxuZXhwb3J0c1sndGVzdCByZWxhdGl2ZSB3ZWJwYWNrIFVSTCByZXNvbHV0aW9uIHdpdGggc291cmNlTWFwVVJMJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBzb3VyY2VzOiAgW1wid2VicGFjay9ib290c3RyYXAuanNcIl0sXG4gICAgbmFtZXM6IFtdLFxuICAgIG1hcHBpbmdzOiBcIkNBQVNcIixcbiAgICBmaWxlOiBcInN0YXRpYy9qcy9tYW5pZmVzdC5iN2NmOTc2ODBmN2E1MGZhMTUwZi5qc1wiLFxuICAgIHNvdXJjZVJvb3Q6IFwid2VicGFjazovLy9cIlxuICB9O1xuICB2YXIgY29uc3VtZXIgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbS9xLmpzLm1hcCcpO1xuXG4gIGFzc2VydC5lcXVhbChjb25zdW1lci5zb3VyY2VzLmxlbmd0aCwgMSk7XG4gIGFzc2VydC5lcXVhbChjb25zdW1lci5zb3VyY2VzWzBdLCBcIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAuanNcIik7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGJhc2ljIFVSTCByZXNvbHV0aW9uIHdpdGggc291cmNlTWFwVVJMJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBzb3VyY2VzOiAgW1wic29tZXRoaW5nLmpzXCJdLFxuICAgIG5hbWVzOiBbXSxcbiAgICBtYXBwaW5nczogXCJDQUFTXCIsXG4gICAgZmlsZTogXCJzdGF0aWMvanMvbWFuaWZlc3QuYjdjZjk3NjgwZjdhNTBmYTE1MGYuanNcIixcbiAgICBzb3VyY2VSb290OiBcInNyY1wiXG4gIH07XG4gIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAsICdodHRwOi8vd3d3LmV4YW1wbGUuY29tL3gvcS5qcy5tYXAnKTtcblxuICBhc3NlcnQuZXF1YWwoY29uc3VtZXIuc291cmNlcy5sZW5ndGgsIDEpO1xuICBhc3NlcnQuZXF1YWwoY29uc3VtZXIuc291cmNlc1swXSwgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20veC9zcmMvc29tZXRoaW5nLmpzJyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGFic29sdXRlIHNvdXJjZVVSTCByZXNvbHV0aW9uIHdpdGggc291cmNlTWFwVVJMJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gIHZhciBtYXAgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBzb3VyY2VzOiAgW1wic29tZXRoaW5nLmpzXCJdLFxuICAgIG5hbWVzOiBbXSxcbiAgICBtYXBwaW5nczogXCJDQUFTXCIsXG4gICAgZmlsZTogXCJzdGF0aWMvanMvbWFuaWZlc3QuYjdjZjk3NjgwZjdhNTBmYTE1MGYuanNcIixcbiAgICBzb3VyY2VSb290OiBcImh0dHA6Ly93d3cuZXhhbXBsZS5jb20vc3JjXCJcbiAgfTtcbiAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcCwgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20veC9xLmpzLm1hcCcpO1xuXG4gIGFzc2VydC5lcXVhbChjb25zdW1lci5zb3VyY2VzLmxlbmd0aCwgMSk7XG4gIGFzc2VydC5lcXVhbChjb25zdW1lci5zb3VyY2VzWzBdLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbS9zcmMvc29tZXRoaW5nLmpzJyk7XG59O1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi90ZXN0L3Rlc3Qtc291cmNlLW1hcC1jb25zdW1lci5qc1xuLy8gbW9kdWxlIGlkID0gMFxuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbnZhciB1dGlsID0gcmVxdWlyZSgnLi4vbGliL3V0aWwnKTtcblxuLy8gVGhpcyBpcyBhIHRlc3QgbWFwcGluZyB3aGljaCBtYXBzIGZ1bmN0aW9ucyBmcm9tIHR3byBkaWZmZXJlbnQgZmlsZXNcbi8vIChvbmUuanMgYW5kIHR3by5qcykgdG8gYSBtaW5pZmllZCBnZW5lcmF0ZWQgc291cmNlLlxuLy9cbi8vIEhlcmUgaXMgb25lLmpzOlxuLy9cbi8vICAgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcbi8vICAgICByZXR1cm4gYmF6KGJhcik7XG4vLyAgIH07XG4vL1xuLy8gSGVyZSBpcyB0d28uanM6XG4vL1xuLy8gICBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcbi8vICAgICByZXR1cm4gbiArIDE7XG4vLyAgIH07XG4vL1xuLy8gQW5kIGhlcmUgaXMgdGhlIGdlbmVyYXRlZCBjb2RlIChtaW4uanMpOlxuLy9cbi8vICAgT05FLmZvbz1mdW5jdGlvbihhKXtyZXR1cm4gYmF6KGEpO307XG4vLyAgIFRXTy5pbmM9ZnVuY3Rpb24oYSl7cmV0dXJuIGErMTt9O1xuZXhwb3J0cy50ZXN0R2VuZXJhdGVkQ29kZSA9IFwiIE9ORS5mb289ZnVuY3Rpb24oYSl7cmV0dXJuIGJheihhKTt9O1xcblwiK1xuICAgICAgICAgICAgICAgICAgICAgICAgICAgIFwiIFRXTy5pbmM9ZnVuY3Rpb24oYSl7cmV0dXJuIGErMTt9O1wiO1xuZXhwb3J0cy50ZXN0TWFwID0ge1xuICB2ZXJzaW9uOiAzLFxuICBmaWxlOiAnbWluLmpzJyxcbiAgbmFtZXM6IFsnYmFyJywgJ2JheicsICduJ10sXG4gIHNvdXJjZXM6IFsnb25lLmpzJywgJ3R3by5qcyddLFxuICBzb3VyY2VSb290OiAnL3RoZS9yb290JyxcbiAgbWFwcGluZ3M6ICdDQUFDLElBQUksSUFBTSxTQUFVQSxHQUNsQixPQUFPQyxJQUFJRDtDQ0RiLElBQUksSUFBTSxTQUFVRSxHQUNsQixPQUFPQSdcbn07XG5leHBvcnRzLnRlc3RNYXBOb1NvdXJjZVJvb3QgPSB7XG4gIHZlcnNpb246IDMsXG4gIGZpbGU6ICdtaW4uanMnLFxuICBuYW1lczogWydiYXInLCAnYmF6JywgJ24nXSxcbiAgc291cmNlczogWydvbmUuanMnLCAndHdvLmpzJ10sXG4gIG1hcHBpbmdzOiAnQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0MsSUFBSUQ7Q0NEYixJQUFJLElBQU0sU0FBVUUsR0FDbEIsT0FBT0EnXG59O1xuZXhwb3J0cy50ZXN0TWFwRW1wdHlTb3VyY2VSb290ID0ge1xuICB2ZXJzaW9uOiAzLFxuICBmaWxlOiAnbWluLmpzJyxcbiAgbmFtZXM6IFsnYmFyJywgJ2JheicsICduJ10sXG4gIHNvdXJjZXM6IFsnb25lLmpzJywgJ3R3by5qcyddLFxuICBzb3VyY2VSb290OiAnJyxcbiAgbWFwcGluZ3M6ICdDQUFDLElBQUksSUFBTSxTQUFVQSxHQUNsQixPQUFPQyxJQUFJRDtDQ0RiLElBQUksSUFBTSxTQUFVRSxHQUNsQixPQUFPQSdcbn07XG5leHBvcnRzLnRlc3RNYXBTaW5nbGVTb3VyY2UgPSB7XG4gIHZlcnNpb246IDMsXG4gIGZpbGU6ICdtaW4uanMnLFxuICBuYW1lczogWydiYXInLCAnYmF6J10sXG4gIHNvdXJjZXM6IFsnb25lLmpzJ10sXG4gIHNvdXJjZVJvb3Q6ICcnLFxuICBtYXBwaW5nczogJ0NBQUMsSUFBSSxJQUFNLFNBQVVBLEdBQ2xCLE9BQU9DLElBQUlEJ1xufTtcbmV4cG9ydHMudGVzdE1hcEVtcHR5TWFwcGluZ3MgPSB7XG4gIHZlcnNpb246IDMsXG4gIGZpbGU6ICdtaW4uanMnLFxuICBuYW1lczogW10sXG4gIHNvdXJjZXM6IFsnb25lLmpzJywgJ3R3by5qcyddLFxuICBzb3VyY2VzQ29udGVudDogW1xuICAgICcgT05FLmZvbyA9IDE7JyxcbiAgICAnIFRXTy5pbmMgPSAyOydcbiAgXSxcbiAgc291cmNlUm9vdDogJycsXG4gIG1hcHBpbmdzOiAnJ1xufTtcbmV4cG9ydHMudGVzdE1hcEVtcHR5TWFwcGluZ3NSZWxhdGl2ZVNvdXJjZXMgPSB7XG4gIHZlcnNpb246IDMsXG4gIGZpbGU6ICdtaW4uanMnLFxuICBuYW1lczogW10sXG4gIHNvdXJjZXM6IFsnLi9vbmUuanMnLCAnLi90d28uanMnXSxcbiAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAnIE9ORS5mb28gPSAxOycsXG4gICAgJyBUV08uaW5jID0gMjsnXG4gIF0sXG4gIHNvdXJjZVJvb3Q6ICcvdGhlL3Jvb3QnLFxuICBtYXBwaW5nczogJydcbn07XG5leHBvcnRzLnRlc3RNYXBFbXB0eU1hcHBpbmdzUmVsYXRpdmVTb3VyY2VzX2dlbmVyYXRlZEV4cGVjdGVkID0ge1xuICB2ZXJzaW9uOiAzLFxuICBmaWxlOiAnbWluLmpzJyxcbiAgbmFtZXM6IFtdLFxuICBzb3VyY2VzOiBbJ29uZS5qcycsICd0d28uanMnXSxcbiAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAnIE9ORS5mb28gPSAxOycsXG4gICAgJyBUV08uaW5jID0gMjsnXG4gIF0sXG4gIHNvdXJjZVJvb3Q6ICcvdGhlL3Jvb3QnLFxuICBtYXBwaW5nczogJydcbn07XG5leHBvcnRzLnRlc3RNYXBNdWx0aVNvdXJjZXNNYXBwaW5nUmVmZXJzU2luZ2xlU291cmNlT25seSA9IHtcbiAgICB2ZXJzaW9uOiAzLFxuICAgIGZpbGU6ICdtaW4uanMnLFxuICAgIG5hbWVzOiBbJ2JhcicsICdiYXonXSxcbiAgICBzb3VyY2VzOiBbJ29uZS5qcycsICd3aXRob3V0TWFwcGluZ3MuanMnXSxcbiAgICBzb3VyY2VSb290OiAnJyxcbiAgICBtYXBwaW5nczogJ0NBQUMsSUFBSSxJQUFNLFNBQVVBLEdBQ2xCLE9BQU9DLElBQUlEJ1xufTtcbi8vIFRoaXMgbWFwcGluZyBpcyBpZGVudGljYWwgdG8gYWJvdmUsIGJ1dCB1c2VzIHRoZSBpbmRleGVkIGZvcm1hdCBpbnN0ZWFkLlxuZXhwb3J0cy5pbmRleGVkVGVzdE1hcCA9IHtcbiAgdmVyc2lvbjogMyxcbiAgZmlsZTogJ21pbi5qcycsXG4gIHNlY3Rpb25zOiBbXG4gICAge1xuICAgICAgb2Zmc2V0OiB7XG4gICAgICAgIGxpbmU6IDAsXG4gICAgICAgIGNvbHVtbjogMFxuICAgICAgfSxcbiAgICAgIG1hcDoge1xuICAgICAgICB2ZXJzaW9uOiAzLFxuICAgICAgICBzb3VyY2VzOiBbXG4gICAgICAgICAgXCJvbmUuanNcIlxuICAgICAgICBdLFxuICAgICAgICBzb3VyY2VzQ29udGVudDogW1xuICAgICAgICAgICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4nICtcbiAgICAgICAgICAnICAgcmV0dXJuIGJheihiYXIpO1xcbicgK1xuICAgICAgICAgICcgfTsnLFxuICAgICAgICBdLFxuICAgICAgICBuYW1lczogW1xuICAgICAgICAgIFwiYmFyXCIsXG4gICAgICAgICAgXCJiYXpcIlxuICAgICAgICBdLFxuICAgICAgICBtYXBwaW5nczogXCJDQUFDLElBQUksSUFBTSxTQUFVQSxHQUNsQixPQUFPQyxJQUFJRFwiLFxuICAgICAgICBmaWxlOiBcIm1pbi5qc1wiLFxuICAgICAgICBzb3VyY2VSb290OiBcIi90aGUvcm9vdFwiXG4gICAgICB9XG4gICAgfSxcbiAgICB7XG4gICAgICBvZmZzZXQ6IHtcbiAgICAgICAgbGluZTogMSxcbiAgICAgICAgY29sdW1uOiAwXG4gICAgICB9LFxuICAgICAgbWFwOiB7XG4gICAgICAgIHZlcnNpb246IDMsXG4gICAgICAgIHNvdXJjZXM6IFtcbiAgICAgICAgICBcInR3by5qc1wiXG4gICAgICAgIF0sXG4gICAgICAgIHNvdXJjZXNDb250ZW50OiBbXG4gICAgICAgICAgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4nICtcbiAgICAgICAgICAnICAgcmV0dXJuIG4gKyAxO1xcbicgK1xuICAgICAgICAgICcgfTsnXG4gICAgICAgIF0sXG4gICAgICAgIG5hbWVzOiBbXG4gICAgICAgICAgXCJuXCJcbiAgICAgICAgXSxcbiAgICAgICAgbWFwcGluZ3M6IFwiQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0FcIixcbiAgICAgICAgZmlsZTogXCJtaW4uanNcIixcbiAgICAgICAgc291cmNlUm9vdDogXCIvdGhlL3Jvb3RcIlxuICAgICAgfVxuICAgIH1cbiAgXVxufTtcbmV4cG9ydHMuaW5kZXhlZFRlc3RNYXBEaWZmZXJlbnRTb3VyY2VSb290cyA9IHtcbiAgdmVyc2lvbjogMyxcbiAgZmlsZTogJ21pbi5qcycsXG4gIHNlY3Rpb25zOiBbXG4gICAge1xuICAgICAgb2Zmc2V0OiB7XG4gICAgICAgIGxpbmU6IDAsXG4gICAgICAgIGNvbHVtbjogMFxuICAgICAgfSxcbiAgICAgIG1hcDoge1xuICAgICAgICB2ZXJzaW9uOiAzLFxuICAgICAgICBzb3VyY2VzOiBbXG4gICAgICAgICAgXCJvbmUuanNcIlxuICAgICAgICBdLFxuICAgICAgICBzb3VyY2VzQ29udGVudDogW1xuICAgICAgICAgICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4nICtcbiAgICAgICAgICAnICAgcmV0dXJuIGJheihiYXIpO1xcbicgK1xuICAgICAgICAgICcgfTsnLFxuICAgICAgICBdLFxuICAgICAgICBuYW1lczogW1xuICAgICAgICAgIFwiYmFyXCIsXG4gICAgICAgICAgXCJiYXpcIlxuICAgICAgICBdLFxuICAgICAgICBtYXBwaW5nczogXCJDQUFDLElBQUksSUFBTSxTQUFVQSxHQUNsQixPQUFPQyxJQUFJRFwiLFxuICAgICAgICBmaWxlOiBcIm1pbi5qc1wiLFxuICAgICAgICBzb3VyY2VSb290OiBcIi90aGUvcm9vdFwiXG4gICAgICB9XG4gICAgfSxcbiAgICB7XG4gICAgICBvZmZzZXQ6IHtcbiAgICAgICAgbGluZTogMSxcbiAgICAgICAgY29sdW1uOiAwXG4gICAgICB9LFxuICAgICAgbWFwOiB7XG4gICAgICAgIHZlcnNpb246IDMsXG4gICAgICAgIHNvdXJjZXM6IFtcbiAgICAgICAgICBcInR3by5qc1wiXG4gICAgICAgIF0sXG4gICAgICAgIHNvdXJjZXNDb250ZW50OiBbXG4gICAgICAgICAgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4nICtcbiAgICAgICAgICAnICAgcmV0dXJuIG4gKyAxO1xcbicgK1xuICAgICAgICAgICcgfTsnXG4gICAgICAgIF0sXG4gICAgICAgIG5hbWVzOiBbXG4gICAgICAgICAgXCJuXCJcbiAgICAgICAgXSxcbiAgICAgICAgbWFwcGluZ3M6IFwiQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0FcIixcbiAgICAgICAgZmlsZTogXCJtaW4uanNcIixcbiAgICAgICAgc291cmNlUm9vdDogXCIvZGlmZmVyZW50L3Jvb3RcIlxuICAgICAgfVxuICAgIH1cbiAgXVxufTtcbmV4cG9ydHMudGVzdE1hcFdpdGhTb3VyY2VzQ29udGVudCA9IHtcbiAgdmVyc2lvbjogMyxcbiAgZmlsZTogJ21pbi5qcycsXG4gIG5hbWVzOiBbJ2JhcicsICdiYXonLCAnbiddLFxuICBzb3VyY2VzOiBbJ29uZS5qcycsICd0d28uanMnXSxcbiAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuJyArXG4gICAgJyAgIHJldHVybiBiYXooYmFyKTtcXG4nICtcbiAgICAnIH07JyxcbiAgICAnIFRXTy5pbmMgPSBmdW5jdGlvbiAobikge1xcbicgK1xuICAgICcgICByZXR1cm4gbiArIDE7XFxuJyArXG4gICAgJyB9OydcbiAgXSxcbiAgc291cmNlUm9vdDogJy90aGUvcm9vdCcsXG4gIG1hcHBpbmdzOiAnQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0MsSUFBSUQ7Q0NEYixJQUFJLElBQU0sU0FBVUUsR0FDbEIsT0FBT0EnXG59O1xuZXhwb3J0cy50ZXN0TWFwUmVsYXRpdmVTb3VyY2VzID0ge1xuICB2ZXJzaW9uOiAzLFxuICBmaWxlOiAnbWluLmpzJyxcbiAgbmFtZXM6IFsnYmFyJywgJ2JheicsICduJ10sXG4gIHNvdXJjZXM6IFsnLi9vbmUuanMnLCAnLi90d28uanMnXSxcbiAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuJyArXG4gICAgJyAgIHJldHVybiBiYXooYmFyKTtcXG4nICtcbiAgICAnIH07JyxcbiAgICAnIFRXTy5pbmMgPSBmdW5jdGlvbiAobikge1xcbicgK1xuICAgICcgICByZXR1cm4gbiArIDE7XFxuJyArXG4gICAgJyB9OydcbiAgXSxcbiAgc291cmNlUm9vdDogJy90aGUvcm9vdCcsXG4gIG1hcHBpbmdzOiAnQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0MsSUFBSUQ7Q0NEYixJQUFJLElBQU0sU0FBVUUsR0FDbEIsT0FBT0EnXG59O1xuZXhwb3J0cy5lbXB0eU1hcCA9IHtcbiAgdmVyc2lvbjogMyxcbiAgZmlsZTogJ21pbi5qcycsXG4gIG5hbWVzOiBbXSxcbiAgc291cmNlczogW10sXG4gIG1hcHBpbmdzOiAnJ1xufTtcblxuXG5mdW5jdGlvbiBhc3NlcnRNYXBwaW5nKGdlbmVyYXRlZExpbmUsIGdlbmVyYXRlZENvbHVtbiwgb3JpZ2luYWxTb3VyY2UsXG4gICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsTGluZSwgb3JpZ2luYWxDb2x1bW4sIG5hbWUsIGJpYXMsIG1hcCwgYXNzZXJ0LFxuICAgICAgICAgICAgICAgICAgICAgICBkb250VGVzdEdlbmVyYXRlZCwgZG9udFRlc3RPcmlnaW5hbCkge1xuICBpZiAoIWRvbnRUZXN0T3JpZ2luYWwpIHtcbiAgICB2YXIgb3JpZ01hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiBnZW5lcmF0ZWRMaW5lLFxuICAgICAgY29sdW1uOiBnZW5lcmF0ZWRDb2x1bW4sXG4gICAgICBiaWFzOiBiaWFzXG4gICAgfSk7XG4gICAgYXNzZXJ0LmVxdWFsKG9yaWdNYXBwaW5nLm5hbWUsIG5hbWUsXG4gICAgICAgICAgICAgICAgICdJbmNvcnJlY3QgbmFtZSwgZXhwZWN0ZWQgJyArIEpTT04uc3RyaW5naWZ5KG5hbWUpXG4gICAgICAgICAgICAgICAgICsgJywgZ290ICcgKyBKU09OLnN0cmluZ2lmeShvcmlnTWFwcGluZy5uYW1lKSk7XG4gICAgYXNzZXJ0LmVxdWFsKG9yaWdNYXBwaW5nLmxpbmUsIG9yaWdpbmFsTGluZSxcbiAgICAgICAgICAgICAgICAgJ0luY29ycmVjdCBsaW5lLCBleHBlY3RlZCAnICsgSlNPTi5zdHJpbmdpZnkob3JpZ2luYWxMaW5lKVxuICAgICAgICAgICAgICAgICArICcsIGdvdCAnICsgSlNPTi5zdHJpbmdpZnkob3JpZ01hcHBpbmcubGluZSkpO1xuICAgIGFzc2VydC5lcXVhbChvcmlnTWFwcGluZy5jb2x1bW4sIG9yaWdpbmFsQ29sdW1uLFxuICAgICAgICAgICAgICAgICAnSW5jb3JyZWN0IGNvbHVtbiwgZXhwZWN0ZWQgJyArIEpTT04uc3RyaW5naWZ5KG9yaWdpbmFsQ29sdW1uKVxuICAgICAgICAgICAgICAgICArICcsIGdvdCAnICsgSlNPTi5zdHJpbmdpZnkob3JpZ01hcHBpbmcuY29sdW1uKSk7XG5cbiAgICB2YXIgZXhwZWN0ZWRTb3VyY2U7XG5cbiAgICBpZiAob3JpZ2luYWxTb3VyY2UgJiYgbWFwLnNvdXJjZVJvb3QgJiYgb3JpZ2luYWxTb3VyY2UuaW5kZXhPZihtYXAuc291cmNlUm9vdCkgPT09IDApIHtcbiAgICAgIGV4cGVjdGVkU291cmNlID0gb3JpZ2luYWxTb3VyY2U7XG4gICAgfSBlbHNlIGlmIChvcmlnaW5hbFNvdXJjZSkge1xuICAgICAgZXhwZWN0ZWRTb3VyY2UgPSBtYXAuc291cmNlUm9vdFxuICAgICAgICA/IHV0aWwuam9pbihtYXAuc291cmNlUm9vdCwgb3JpZ2luYWxTb3VyY2UpXG4gICAgICAgIDogb3JpZ2luYWxTb3VyY2U7XG4gICAgfSBlbHNlIHtcbiAgICAgIGV4cGVjdGVkU291cmNlID0gbnVsbDtcbiAgICB9XG5cbiAgICBhc3NlcnQuZXF1YWwob3JpZ01hcHBpbmcuc291cmNlLCBleHBlY3RlZFNvdXJjZSxcbiAgICAgICAgICAgICAgICAgJ0luY29ycmVjdCBzb3VyY2UsIGV4cGVjdGVkICcgKyBKU09OLnN0cmluZ2lmeShleHBlY3RlZFNvdXJjZSlcbiAgICAgICAgICAgICAgICAgKyAnLCBnb3QgJyArIEpTT04uc3RyaW5naWZ5KG9yaWdNYXBwaW5nLnNvdXJjZSkpO1xuICB9XG5cbiAgaWYgKCFkb250VGVzdEdlbmVyYXRlZCkge1xuICAgIHZhciBnZW5NYXBwaW5nID0gbWFwLmdlbmVyYXRlZFBvc2l0aW9uRm9yKHtcbiAgICAgIHNvdXJjZTogb3JpZ2luYWxTb3VyY2UsXG4gICAgICBsaW5lOiBvcmlnaW5hbExpbmUsXG4gICAgICBjb2x1bW46IG9yaWdpbmFsQ29sdW1uLFxuICAgICAgYmlhczogYmlhc1xuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChnZW5NYXBwaW5nLmxpbmUsIGdlbmVyYXRlZExpbmUsXG4gICAgICAgICAgICAgICAgICdJbmNvcnJlY3QgbGluZSwgZXhwZWN0ZWQgJyArIEpTT04uc3RyaW5naWZ5KGdlbmVyYXRlZExpbmUpXG4gICAgICAgICAgICAgICAgICsgJywgZ290ICcgKyBKU09OLnN0cmluZ2lmeShnZW5NYXBwaW5nLmxpbmUpKTtcbiAgICBhc3NlcnQuZXF1YWwoZ2VuTWFwcGluZy5jb2x1bW4sIGdlbmVyYXRlZENvbHVtbixcbiAgICAgICAgICAgICAgICAgJ0luY29ycmVjdCBjb2x1bW4sIGV4cGVjdGVkICcgKyBKU09OLnN0cmluZ2lmeShnZW5lcmF0ZWRDb2x1bW4pXG4gICAgICAgICAgICAgICAgICsgJywgZ290ICcgKyBKU09OLnN0cmluZ2lmeShnZW5NYXBwaW5nLmNvbHVtbikpO1xuICB9XG59XG5leHBvcnRzLmFzc2VydE1hcHBpbmcgPSBhc3NlcnRNYXBwaW5nO1xuXG5mdW5jdGlvbiBhc3NlcnRFcXVhbE1hcHMoYXNzZXJ0LCBhY3R1YWxNYXAsIGV4cGVjdGVkTWFwKSB7XG4gIGFzc2VydC5lcXVhbChhY3R1YWxNYXAudmVyc2lvbiwgZXhwZWN0ZWRNYXAudmVyc2lvbiwgXCJ2ZXJzaW9uIG1pc21hdGNoXCIpO1xuICBhc3NlcnQuZXF1YWwoYWN0dWFsTWFwLmZpbGUsIGV4cGVjdGVkTWFwLmZpbGUsIFwiZmlsZSBtaXNtYXRjaFwiKTtcbiAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5uYW1lcy5sZW5ndGgsXG4gICAgICAgICAgICAgICBleHBlY3RlZE1hcC5uYW1lcy5sZW5ndGgsXG4gICAgICAgICAgICAgICBcIm5hbWVzIGxlbmd0aCBtaXNtYXRjaDogXCIgK1xuICAgICAgICAgICAgICAgICBhY3R1YWxNYXAubmFtZXMuam9pbihcIiwgXCIpICsgXCIgIT0gXCIgKyBleHBlY3RlZE1hcC5uYW1lcy5qb2luKFwiLCBcIikpO1xuICBmb3IgKHZhciBpID0gMDsgaSA8IGFjdHVhbE1hcC5uYW1lcy5sZW5ndGg7IGkrKykge1xuICAgIGFzc2VydC5lcXVhbChhY3R1YWxNYXAubmFtZXNbaV0sXG4gICAgICAgICAgICAgICAgIGV4cGVjdGVkTWFwLm5hbWVzW2ldLFxuICAgICAgICAgICAgICAgICBcIm5hbWVzW1wiICsgaSArIFwiXSBtaXNtYXRjaDogXCIgK1xuICAgICAgICAgICAgICAgICAgIGFjdHVhbE1hcC5uYW1lcy5qb2luKFwiLCBcIikgKyBcIiAhPSBcIiArIGV4cGVjdGVkTWFwLm5hbWVzLmpvaW4oXCIsIFwiKSk7XG4gIH1cbiAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5zb3VyY2VzLmxlbmd0aCxcbiAgICAgICAgICAgICAgIGV4cGVjdGVkTWFwLnNvdXJjZXMubGVuZ3RoLFxuICAgICAgICAgICAgICAgXCJzb3VyY2VzIGxlbmd0aCBtaXNtYXRjaDogXCIgK1xuICAgICAgICAgICAgICAgICBhY3R1YWxNYXAuc291cmNlcy5qb2luKFwiLCBcIikgKyBcIiAhPSBcIiArIGV4cGVjdGVkTWFwLnNvdXJjZXMuam9pbihcIiwgXCIpKTtcbiAgZm9yICh2YXIgaSA9IDA7IGkgPCBhY3R1YWxNYXAuc291cmNlcy5sZW5ndGg7IGkrKykge1xuICAgIGFzc2VydC5lcXVhbChhY3R1YWxNYXAuc291cmNlc1tpXSxcbiAgICAgICAgICAgICAgICAgZXhwZWN0ZWRNYXAuc291cmNlc1tpXSxcbiAgICAgICAgICAgICAgICAgXCJzb3VyY2VzW1wiICsgaSArIFwiXSBsZW5ndGggbWlzbWF0Y2g6IFwiICtcbiAgICAgICAgICAgICAgICAgYWN0dWFsTWFwLnNvdXJjZXMuam9pbihcIiwgXCIpICsgXCIgIT0gXCIgKyBleHBlY3RlZE1hcC5zb3VyY2VzLmpvaW4oXCIsIFwiKSk7XG4gIH1cbiAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5zb3VyY2VSb290LFxuICAgICAgICAgICAgICAgZXhwZWN0ZWRNYXAuc291cmNlUm9vdCxcbiAgICAgICAgICAgICAgIFwic291cmNlUm9vdCBtaXNtYXRjaDogXCIgK1xuICAgICAgICAgICAgICAgICBhY3R1YWxNYXAuc291cmNlUm9vdCArIFwiICE9IFwiICsgZXhwZWN0ZWRNYXAuc291cmNlUm9vdCk7XG4gIGFzc2VydC5lcXVhbChhY3R1YWxNYXAubWFwcGluZ3MsIGV4cGVjdGVkTWFwLm1hcHBpbmdzLFxuICAgICAgICAgICAgICAgXCJtYXBwaW5ncyBtaXNtYXRjaDpcXG5BY3R1YWw6ICAgXCIgKyBhY3R1YWxNYXAubWFwcGluZ3MgKyBcIlxcbkV4cGVjdGVkOiBcIiArIGV4cGVjdGVkTWFwLm1hcHBpbmdzKTtcbiAgaWYgKGFjdHVhbE1hcC5zb3VyY2VzQ29udGVudCkge1xuICAgIGFzc2VydC5lcXVhbChhY3R1YWxNYXAuc291cmNlc0NvbnRlbnQubGVuZ3RoLFxuICAgICAgICAgICAgICAgICBleHBlY3RlZE1hcC5zb3VyY2VzQ29udGVudC5sZW5ndGgsXG4gICAgICAgICAgICAgICAgIFwic291cmNlc0NvbnRlbnQgbGVuZ3RoIG1pc21hdGNoXCIpO1xuICAgIGZvciAodmFyIGkgPSAwOyBpIDwgYWN0dWFsTWFwLnNvdXJjZXNDb250ZW50Lmxlbmd0aDsgaSsrKSB7XG4gICAgICBhc3NlcnQuZXF1YWwoYWN0dWFsTWFwLnNvdXJjZXNDb250ZW50W2ldLFxuICAgICAgICAgICAgICAgICAgIGV4cGVjdGVkTWFwLnNvdXJjZXNDb250ZW50W2ldLFxuICAgICAgICAgICAgICAgICAgIFwic291cmNlc0NvbnRlbnRbXCIgKyBpICsgXCJdIG1pc21hdGNoXCIpO1xuICAgIH1cbiAgfVxufVxuZXhwb3J0cy5hc3NlcnRFcXVhbE1hcHMgPSBhc3NlcnRFcXVhbE1hcHM7XG5cblxuXG4vLy8vLy8vLy8vLy8vLy8vLy9cbi8vIFdFQlBBQ0sgRk9PVEVSXG4vLyAuL3Rlc3QvdXRpbC5qc1xuLy8gbW9kdWxlIGlkID0gMVxuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbi8qKlxuICogVGhpcyBpcyBhIGhlbHBlciBmdW5jdGlvbiBmb3IgZ2V0dGluZyB2YWx1ZXMgZnJvbSBwYXJhbWV0ZXIvb3B0aW9uc1xuICogb2JqZWN0cy5cbiAqXG4gKiBAcGFyYW0gYXJncyBUaGUgb2JqZWN0IHdlIGFyZSBleHRyYWN0aW5nIHZhbHVlcyBmcm9tXG4gKiBAcGFyYW0gbmFtZSBUaGUgbmFtZSBvZiB0aGUgcHJvcGVydHkgd2UgYXJlIGdldHRpbmcuXG4gKiBAcGFyYW0gZGVmYXVsdFZhbHVlIEFuIG9wdGlvbmFsIHZhbHVlIHRvIHJldHVybiBpZiB0aGUgcHJvcGVydHkgaXMgbWlzc2luZ1xuICogZnJvbSB0aGUgb2JqZWN0LiBJZiB0aGlzIGlzIG5vdCBzcGVjaWZpZWQgYW5kIHRoZSBwcm9wZXJ0eSBpcyBtaXNzaW5nLCBhblxuICogZXJyb3Igd2lsbCBiZSB0aHJvd24uXG4gKi9cbmZ1bmN0aW9uIGdldEFyZyhhQXJncywgYU5hbWUsIGFEZWZhdWx0VmFsdWUpIHtcbiAgaWYgKGFOYW1lIGluIGFBcmdzKSB7XG4gICAgcmV0dXJuIGFBcmdzW2FOYW1lXTtcbiAgfSBlbHNlIGlmIChhcmd1bWVudHMubGVuZ3RoID09PSAzKSB7XG4gICAgcmV0dXJuIGFEZWZhdWx0VmFsdWU7XG4gIH0gZWxzZSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhTmFtZSArICdcIiBpcyBhIHJlcXVpcmVkIGFyZ3VtZW50LicpO1xuICB9XG59XG5leHBvcnRzLmdldEFyZyA9IGdldEFyZztcblxudmFyIHVybFJlZ2V4cCA9IC9eKD86KFtcXHcrXFwtLl0rKTopP1xcL1xcLyg/OihcXHcrOlxcdyspQCk/KFtcXHcuLV0qKSg/OjooXFxkKykpPyguKikkLztcbnZhciBkYXRhVXJsUmVnZXhwID0gL15kYXRhOi4rXFwsLiskLztcblxuZnVuY3Rpb24gdXJsUGFyc2UoYVVybCkge1xuICB2YXIgbWF0Y2ggPSBhVXJsLm1hdGNoKHVybFJlZ2V4cCk7XG4gIGlmICghbWF0Y2gpIHtcbiAgICByZXR1cm4gbnVsbDtcbiAgfVxuICByZXR1cm4ge1xuICAgIHNjaGVtZTogbWF0Y2hbMV0sXG4gICAgYXV0aDogbWF0Y2hbMl0sXG4gICAgaG9zdDogbWF0Y2hbM10sXG4gICAgcG9ydDogbWF0Y2hbNF0sXG4gICAgcGF0aDogbWF0Y2hbNV1cbiAgfTtcbn1cbmV4cG9ydHMudXJsUGFyc2UgPSB1cmxQYXJzZTtcblxuZnVuY3Rpb24gdXJsR2VuZXJhdGUoYVBhcnNlZFVybCkge1xuICB2YXIgdXJsID0gJyc7XG4gIGlmIChhUGFyc2VkVXJsLnNjaGVtZSkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLnNjaGVtZSArICc6JztcbiAgfVxuICB1cmwgKz0gJy8vJztcbiAgaWYgKGFQYXJzZWRVcmwuYXV0aCkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLmF1dGggKyAnQCc7XG4gIH1cbiAgaWYgKGFQYXJzZWRVcmwuaG9zdCkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLmhvc3Q7XG4gIH1cbiAgaWYgKGFQYXJzZWRVcmwucG9ydCkge1xuICAgIHVybCArPSBcIjpcIiArIGFQYXJzZWRVcmwucG9ydFxuICB9XG4gIGlmIChhUGFyc2VkVXJsLnBhdGgpIHtcbiAgICB1cmwgKz0gYVBhcnNlZFVybC5wYXRoO1xuICB9XG4gIHJldHVybiB1cmw7XG59XG5leHBvcnRzLnVybEdlbmVyYXRlID0gdXJsR2VuZXJhdGU7XG5cbi8qKlxuICogTm9ybWFsaXplcyBhIHBhdGgsIG9yIHRoZSBwYXRoIHBvcnRpb24gb2YgYSBVUkw6XG4gKlxuICogLSBSZXBsYWNlcyBjb25zZWN1dGl2ZSBzbGFzaGVzIHdpdGggb25lIHNsYXNoLlxuICogLSBSZW1vdmVzIHVubmVjZXNzYXJ5ICcuJyBwYXJ0cy5cbiAqIC0gUmVtb3ZlcyB1bm5lY2Vzc2FyeSAnPGRpcj4vLi4nIHBhcnRzLlxuICpcbiAqIEJhc2VkIG9uIGNvZGUgaW4gdGhlIE5vZGUuanMgJ3BhdGgnIGNvcmUgbW9kdWxlLlxuICpcbiAqIEBwYXJhbSBhUGF0aCBUaGUgcGF0aCBvciB1cmwgdG8gbm9ybWFsaXplLlxuICovXG5mdW5jdGlvbiBub3JtYWxpemUoYVBhdGgpIHtcbiAgdmFyIHBhdGggPSBhUGF0aDtcbiAgdmFyIHVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgaWYgKHVybCkge1xuICAgIGlmICghdXJsLnBhdGgpIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG4gICAgcGF0aCA9IHVybC5wYXRoO1xuICB9XG4gIHZhciBpc0Fic29sdXRlID0gZXhwb3J0cy5pc0Fic29sdXRlKHBhdGgpO1xuXG4gIHZhciBwYXJ0cyA9IHBhdGguc3BsaXQoL1xcLysvKTtcbiAgZm9yICh2YXIgcGFydCwgdXAgPSAwLCBpID0gcGFydHMubGVuZ3RoIC0gMTsgaSA+PSAwOyBpLS0pIHtcbiAgICBwYXJ0ID0gcGFydHNbaV07XG4gICAgaWYgKHBhcnQgPT09ICcuJykge1xuICAgICAgcGFydHMuc3BsaWNlKGksIDEpO1xuICAgIH0gZWxzZSBpZiAocGFydCA9PT0gJy4uJykge1xuICAgICAgdXArKztcbiAgICB9IGVsc2UgaWYgKHVwID4gMCkge1xuICAgICAgaWYgKHBhcnQgPT09ICcnKSB7XG4gICAgICAgIC8vIFRoZSBmaXJzdCBwYXJ0IGlzIGJsYW5rIGlmIHRoZSBwYXRoIGlzIGFic29sdXRlLiBUcnlpbmcgdG8gZ29cbiAgICAgICAgLy8gYWJvdmUgdGhlIHJvb3QgaXMgYSBuby1vcC4gVGhlcmVmb3JlIHdlIGNhbiByZW1vdmUgYWxsICcuLicgcGFydHNcbiAgICAgICAgLy8gZGlyZWN0bHkgYWZ0ZXIgdGhlIHJvb3QuXG4gICAgICAgIHBhcnRzLnNwbGljZShpICsgMSwgdXApO1xuICAgICAgICB1cCA9IDA7XG4gICAgICB9IGVsc2Uge1xuICAgICAgICBwYXJ0cy5zcGxpY2UoaSwgMik7XG4gICAgICAgIHVwLS07XG4gICAgICB9XG4gICAgfVxuICB9XG4gIHBhdGggPSBwYXJ0cy5qb2luKCcvJyk7XG5cbiAgaWYgKHBhdGggPT09ICcnKSB7XG4gICAgcGF0aCA9IGlzQWJzb2x1dGUgPyAnLycgOiAnLic7XG4gIH1cblxuICBpZiAodXJsKSB7XG4gICAgdXJsLnBhdGggPSBwYXRoO1xuICAgIHJldHVybiB1cmxHZW5lcmF0ZSh1cmwpO1xuICB9XG4gIHJldHVybiBwYXRoO1xufVxuZXhwb3J0cy5ub3JtYWxpemUgPSBub3JtYWxpemU7XG5cbi8qKlxuICogSm9pbnMgdHdvIHBhdGhzL1VSTHMuXG4gKlxuICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBqb2luZWQgd2l0aCB0aGUgcm9vdC5cbiAqXG4gKiAtIElmIGFQYXRoIGlzIGEgVVJMIG9yIGEgZGF0YSBVUkksIGFQYXRoIGlzIHJldHVybmVkLCB1bmxlc3MgYVBhdGggaXMgYVxuICogICBzY2hlbWUtcmVsYXRpdmUgVVJMOiBUaGVuIHRoZSBzY2hlbWUgb2YgYVJvb3QsIGlmIGFueSwgaXMgcHJlcGVuZGVkXG4gKiAgIGZpcnN0LlxuICogLSBPdGhlcndpc2UgYVBhdGggaXMgYSBwYXRoLiBJZiBhUm9vdCBpcyBhIFVSTCwgdGhlbiBpdHMgcGF0aCBwb3J0aW9uXG4gKiAgIGlzIHVwZGF0ZWQgd2l0aCB0aGUgcmVzdWx0IGFuZCBhUm9vdCBpcyByZXR1cm5lZC4gT3RoZXJ3aXNlIHRoZSByZXN1bHRcbiAqICAgaXMgcmV0dXJuZWQuXG4gKiAgIC0gSWYgYVBhdGggaXMgYWJzb2x1dGUsIHRoZSByZXN1bHQgaXMgYVBhdGguXG4gKiAgIC0gT3RoZXJ3aXNlIHRoZSB0d28gcGF0aHMgYXJlIGpvaW5lZCB3aXRoIGEgc2xhc2guXG4gKiAtIEpvaW5pbmcgZm9yIGV4YW1wbGUgJ2h0dHA6Ly8nIGFuZCAnd3d3LmV4YW1wbGUuY29tJyBpcyBhbHNvIHN1cHBvcnRlZC5cbiAqL1xuZnVuY3Rpb24gam9pbihhUm9vdCwgYVBhdGgpIHtcbiAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgYVJvb3QgPSBcIi5cIjtcbiAgfVxuICBpZiAoYVBhdGggPT09IFwiXCIpIHtcbiAgICBhUGF0aCA9IFwiLlwiO1xuICB9XG4gIHZhciBhUGF0aFVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgdmFyIGFSb290VXJsID0gdXJsUGFyc2UoYVJvb3QpO1xuICBpZiAoYVJvb3RVcmwpIHtcbiAgICBhUm9vdCA9IGFSb290VXJsLnBhdGggfHwgJy8nO1xuICB9XG5cbiAgLy8gYGpvaW4oZm9vLCAnLy93d3cuZXhhbXBsZS5vcmcnKWBcbiAgaWYgKGFQYXRoVXJsICYmICFhUGF0aFVybC5zY2hlbWUpIHtcbiAgICBpZiAoYVJvb3RVcmwpIHtcbiAgICAgIGFQYXRoVXJsLnNjaGVtZSA9IGFSb290VXJsLnNjaGVtZTtcbiAgICB9XG4gICAgcmV0dXJuIHVybEdlbmVyYXRlKGFQYXRoVXJsKTtcbiAgfVxuXG4gIGlmIChhUGF0aFVybCB8fCBhUGF0aC5tYXRjaChkYXRhVXJsUmVnZXhwKSkge1xuICAgIHJldHVybiBhUGF0aDtcbiAgfVxuXG4gIC8vIGBqb2luKCdodHRwOi8vJywgJ3d3dy5leGFtcGxlLmNvbScpYFxuICBpZiAoYVJvb3RVcmwgJiYgIWFSb290VXJsLmhvc3QgJiYgIWFSb290VXJsLnBhdGgpIHtcbiAgICBhUm9vdFVybC5ob3N0ID0gYVBhdGg7XG4gICAgcmV0dXJuIHVybEdlbmVyYXRlKGFSb290VXJsKTtcbiAgfVxuXG4gIHZhciBqb2luZWQgPSBhUGF0aC5jaGFyQXQoMCkgPT09ICcvJ1xuICAgID8gYVBhdGhcbiAgICA6IG5vcm1hbGl6ZShhUm9vdC5yZXBsYWNlKC9cXC8rJC8sICcnKSArICcvJyArIGFQYXRoKTtcblxuICBpZiAoYVJvb3RVcmwpIHtcbiAgICBhUm9vdFVybC5wYXRoID0gam9pbmVkO1xuICAgIHJldHVybiB1cmxHZW5lcmF0ZShhUm9vdFVybCk7XG4gIH1cbiAgcmV0dXJuIGpvaW5lZDtcbn1cbmV4cG9ydHMuam9pbiA9IGpvaW47XG5cbmV4cG9ydHMuaXNBYnNvbHV0ZSA9IGZ1bmN0aW9uIChhUGF0aCkge1xuICByZXR1cm4gYVBhdGguY2hhckF0KDApID09PSAnLycgfHwgdXJsUmVnZXhwLnRlc3QoYVBhdGgpO1xufTtcblxuLyoqXG4gKiBNYWtlIGEgcGF0aCByZWxhdGl2ZSB0byBhIFVSTCBvciBhbm90aGVyIHBhdGguXG4gKlxuICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBtYWRlIHJlbGF0aXZlIHRvIGFSb290LlxuICovXG5mdW5jdGlvbiByZWxhdGl2ZShhUm9vdCwgYVBhdGgpIHtcbiAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgYVJvb3QgPSBcIi5cIjtcbiAgfVxuXG4gIGFSb290ID0gYVJvb3QucmVwbGFjZSgvXFwvJC8sICcnKTtcblxuICAvLyBJdCBpcyBwb3NzaWJsZSBmb3IgdGhlIHBhdGggdG8gYmUgYWJvdmUgdGhlIHJvb3QuIEluIHRoaXMgY2FzZSwgc2ltcGx5XG4gIC8vIGNoZWNraW5nIHdoZXRoZXIgdGhlIHJvb3QgaXMgYSBwcmVmaXggb2YgdGhlIHBhdGggd29uJ3Qgd29yay4gSW5zdGVhZCwgd2VcbiAgLy8gbmVlZCB0byByZW1vdmUgY29tcG9uZW50cyBmcm9tIHRoZSByb290IG9uZSBieSBvbmUsIHVudGlsIGVpdGhlciB3ZSBmaW5kXG4gIC8vIGEgcHJlZml4IHRoYXQgZml0cywgb3Igd2UgcnVuIG91dCBvZiBjb21wb25lbnRzIHRvIHJlbW92ZS5cbiAgdmFyIGxldmVsID0gMDtcbiAgd2hpbGUgKGFQYXRoLmluZGV4T2YoYVJvb3QgKyAnLycpICE9PSAwKSB7XG4gICAgdmFyIGluZGV4ID0gYVJvb3QubGFzdEluZGV4T2YoXCIvXCIpO1xuICAgIGlmIChpbmRleCA8IDApIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG5cbiAgICAvLyBJZiB0aGUgb25seSBwYXJ0IG9mIHRoZSByb290IHRoYXQgaXMgbGVmdCBpcyB0aGUgc2NoZW1lIChpLmUuIGh0dHA6Ly8sXG4gICAgLy8gZmlsZTovLy8sIGV0Yy4pLCBvbmUgb3IgbW9yZSBzbGFzaGVzICgvKSwgb3Igc2ltcGx5IG5vdGhpbmcgYXQgYWxsLCB3ZVxuICAgIC8vIGhhdmUgZXhoYXVzdGVkIGFsbCBjb21wb25lbnRzLCBzbyB0aGUgcGF0aCBpcyBub3QgcmVsYXRpdmUgdG8gdGhlIHJvb3QuXG4gICAgYVJvb3QgPSBhUm9vdC5zbGljZSgwLCBpbmRleCk7XG4gICAgaWYgKGFSb290Lm1hdGNoKC9eKFteXFwvXSs6XFwvKT9cXC8qJC8pKSB7XG4gICAgICByZXR1cm4gYVBhdGg7XG4gICAgfVxuXG4gICAgKytsZXZlbDtcbiAgfVxuXG4gIC8vIE1ha2Ugc3VyZSB3ZSBhZGQgYSBcIi4uL1wiIGZvciBlYWNoIGNvbXBvbmVudCB3ZSByZW1vdmVkIGZyb20gdGhlIHJvb3QuXG4gIHJldHVybiBBcnJheShsZXZlbCArIDEpLmpvaW4oXCIuLi9cIikgKyBhUGF0aC5zdWJzdHIoYVJvb3QubGVuZ3RoICsgMSk7XG59XG5leHBvcnRzLnJlbGF0aXZlID0gcmVsYXRpdmU7XG5cbnZhciBzdXBwb3J0c051bGxQcm90byA9IChmdW5jdGlvbiAoKSB7XG4gIHZhciBvYmogPSBPYmplY3QuY3JlYXRlKG51bGwpO1xuICByZXR1cm4gISgnX19wcm90b19fJyBpbiBvYmopO1xufSgpKTtcblxuZnVuY3Rpb24gaWRlbnRpdHkgKHMpIHtcbiAgcmV0dXJuIHM7XG59XG5cbi8qKlxuICogQmVjYXVzZSBiZWhhdmlvciBnb2VzIHdhY2t5IHdoZW4geW91IHNldCBgX19wcm90b19fYCBvbiBvYmplY3RzLCB3ZVxuICogaGF2ZSB0byBwcmVmaXggYWxsIHRoZSBzdHJpbmdzIGluIG91ciBzZXQgd2l0aCBhbiBhcmJpdHJhcnkgY2hhcmFjdGVyLlxuICpcbiAqIFNlZSBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL3B1bGwvMzEgYW5kXG4gKiBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL2lzc3Vlcy8zMFxuICpcbiAqIEBwYXJhbSBTdHJpbmcgYVN0clxuICovXG5mdW5jdGlvbiB0b1NldFN0cmluZyhhU3RyKSB7XG4gIGlmIChpc1Byb3RvU3RyaW5nKGFTdHIpKSB7XG4gICAgcmV0dXJuICckJyArIGFTdHI7XG4gIH1cblxuICByZXR1cm4gYVN0cjtcbn1cbmV4cG9ydHMudG9TZXRTdHJpbmcgPSBzdXBwb3J0c051bGxQcm90byA/IGlkZW50aXR5IDogdG9TZXRTdHJpbmc7XG5cbmZ1bmN0aW9uIGZyb21TZXRTdHJpbmcoYVN0cikge1xuICBpZiAoaXNQcm90b1N0cmluZyhhU3RyKSkge1xuICAgIHJldHVybiBhU3RyLnNsaWNlKDEpO1xuICB9XG5cbiAgcmV0dXJuIGFTdHI7XG59XG5leHBvcnRzLmZyb21TZXRTdHJpbmcgPSBzdXBwb3J0c051bGxQcm90byA/IGlkZW50aXR5IDogZnJvbVNldFN0cmluZztcblxuZnVuY3Rpb24gaXNQcm90b1N0cmluZyhzKSB7XG4gIGlmICghcykge1xuICAgIHJldHVybiBmYWxzZTtcbiAgfVxuXG4gIHZhciBsZW5ndGggPSBzLmxlbmd0aDtcblxuICBpZiAobGVuZ3RoIDwgOSAvKiBcIl9fcHJvdG9fX1wiLmxlbmd0aCAqLykge1xuICAgIHJldHVybiBmYWxzZTtcbiAgfVxuXG4gIGlmIChzLmNoYXJDb2RlQXQobGVuZ3RoIC0gMSkgIT09IDk1ICAvKiAnXycgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSAyKSAhPT0gOTUgIC8qICdfJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDMpICE9PSAxMTEgLyogJ28nICovIHx8XG4gICAgICBzLmNoYXJDb2RlQXQobGVuZ3RoIC0gNCkgIT09IDExNiAvKiAndCcgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSA1KSAhPT0gMTExIC8qICdvJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDYpICE9PSAxMTQgLyogJ3InICovIHx8XG4gICAgICBzLmNoYXJDb2RlQXQobGVuZ3RoIC0gNykgIT09IDExMiAvKiAncCcgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSA4KSAhPT0gOTUgIC8qICdfJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDkpICE9PSA5NSAgLyogJ18nICovKSB7XG4gICAgcmV0dXJuIGZhbHNlO1xuICB9XG5cbiAgZm9yICh2YXIgaSA9IGxlbmd0aCAtIDEwOyBpID49IDA7IGktLSkge1xuICAgIGlmIChzLmNoYXJDb2RlQXQoaSkgIT09IDM2IC8qICckJyAqLykge1xuICAgICAgcmV0dXJuIGZhbHNlO1xuICAgIH1cbiAgfVxuXG4gIHJldHVybiB0cnVlO1xufVxuXG4vKipcbiAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2hlcmUgdGhlIG9yaWdpbmFsIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKlxuICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4sIGJ1dCBkaWZmZXJlbnQgZ2VuZXJhdGVkXG4gKiBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYSBtYXBwaW5nIHdpdGggYVxuICogc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAqL1xuZnVuY3Rpb24gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZU9yaWdpbmFsKSB7XG4gIHZhciBjbXAgPSBzdHJjbXAobWFwcGluZ0Euc291cmNlLCBtYXBwaW5nQi5zb3VyY2UpO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsTGluZSAtIG1hcHBpbmdCLm9yaWdpbmFsTGluZTtcbiAgaWYgKGNtcCAhPT0gMCkge1xuICAgIHJldHVybiBjbXA7XG4gIH1cblxuICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbENvbHVtbiAtIG1hcHBpbmdCLm9yaWdpbmFsQ29sdW1uO1xuICBpZiAoY21wICE9PSAwIHx8IG9ubHlDb21wYXJlT3JpZ2luYWwpIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmUgLSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIHJldHVybiBzdHJjbXAobWFwcGluZ0EubmFtZSwgbWFwcGluZ0IubmFtZSk7XG59XG5leHBvcnRzLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zID0gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnM7XG5cbi8qKlxuICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGRlZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBpbmRpY2VzIHdoZXJlXG4gKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKlxuICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uLCBidXQgZGlmZmVyZW50XG4gKiBzb3VyY2UvbmFtZS9vcmlnaW5hbCBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYVxuICogbWFwcGluZyB3aXRoIGEgc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAqL1xuZnVuY3Rpb24gY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZUdlbmVyYXRlZCkge1xuICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwIHx8IG9ubHlDb21wYXJlR2VuZXJhdGVkKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IHN0cmNtcChtYXBwaW5nQS5zb3VyY2UsIG1hcHBpbmdCLnNvdXJjZSk7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIHN0cmNtcChtYXBwaW5nQS5uYW1lLCBtYXBwaW5nQi5uYW1lKTtcbn1cbmV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZDtcblxuZnVuY3Rpb24gc3RyY21wKGFTdHIxLCBhU3RyMikge1xuICBpZiAoYVN0cjEgPT09IGFTdHIyKSB7XG4gICAgcmV0dXJuIDA7XG4gIH1cblxuICBpZiAoYVN0cjEgPT09IG51bGwpIHtcbiAgICByZXR1cm4gMTsgLy8gYVN0cjIgIT09IG51bGxcbiAgfVxuXG4gIGlmIChhU3RyMiA9PT0gbnVsbCkge1xuICAgIHJldHVybiAtMTsgLy8gYVN0cjEgIT09IG51bGxcbiAgfVxuXG4gIGlmIChhU3RyMSA+IGFTdHIyKSB7XG4gICAgcmV0dXJuIDE7XG4gIH1cblxuICByZXR1cm4gLTE7XG59XG5cbi8qKlxuICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGluZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBzdHJpbmdzIHdoZXJlXG4gKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKi9cbmZ1bmN0aW9uIGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQikge1xuICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IHN0cmNtcChtYXBwaW5nQS5zb3VyY2UsIG1hcHBpbmdCLnNvdXJjZSk7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIHN0cmNtcChtYXBwaW5nQS5uYW1lLCBtYXBwaW5nQi5uYW1lKTtcbn1cbmV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZDtcblxuLyoqXG4gKiBTdHJpcCBhbnkgSlNPTiBYU1NJIGF2b2lkYW5jZSBwcmVmaXggZnJvbSB0aGUgc3RyaW5nIChhcyBkb2N1bWVudGVkXG4gKiBpbiB0aGUgc291cmNlIG1hcHMgc3BlY2lmaWNhdGlvbiksIGFuZCB0aGVuIHBhcnNlIHRoZSBzdHJpbmcgYXNcbiAqIEpTT04uXG4gKi9cbmZ1bmN0aW9uIHBhcnNlU291cmNlTWFwSW5wdXQoc3RyKSB7XG4gIHJldHVybiBKU09OLnBhcnNlKHN0ci5yZXBsYWNlKC9eXFwpXX0nW15cXG5dKlxcbi8sICcnKSk7XG59XG5leHBvcnRzLnBhcnNlU291cmNlTWFwSW5wdXQgPSBwYXJzZVNvdXJjZU1hcElucHV0O1xuXG4vKipcbiAqIENvbXB1dGUgdGhlIFVSTCBvZiBhIHNvdXJjZSBnaXZlbiB0aGUgdGhlIHNvdXJjZSByb290LCB0aGUgc291cmNlJ3NcbiAqIFVSTCwgYW5kIHRoZSBzb3VyY2UgbWFwJ3MgVVJMLlxuICovXG5mdW5jdGlvbiBjb21wdXRlU291cmNlVVJMKHNvdXJjZVJvb3QsIHNvdXJjZVVSTCwgc291cmNlTWFwVVJMKSB7XG4gIHNvdXJjZVVSTCA9IHNvdXJjZVVSTCB8fCAnJztcblxuICBpZiAoc291cmNlUm9vdCkge1xuICAgIC8vIFRoaXMgZm9sbG93cyB3aGF0IENocm9tZSBkb2VzLlxuICAgIGlmIChzb3VyY2VSb290W3NvdXJjZVJvb3QubGVuZ3RoIC0gMV0gIT09ICcvJyAmJiBzb3VyY2VVUkxbMF0gIT09ICcvJykge1xuICAgICAgc291cmNlUm9vdCArPSAnLyc7XG4gICAgfVxuICAgIC8vIFRoZSBzcGVjIHNheXM6XG4gICAgLy8gICBMaW5lIDQ6IEFuIG9wdGlvbmFsIHNvdXJjZSByb290LCB1c2VmdWwgZm9yIHJlbG9jYXRpbmcgc291cmNlXG4gICAgLy8gICBmaWxlcyBvbiBhIHNlcnZlciBvciByZW1vdmluZyByZXBlYXRlZCB2YWx1ZXMgaW4gdGhlXG4gICAgLy8gICDigJxzb3VyY2Vz4oCdIGVudHJ5LiAgVGhpcyB2YWx1ZSBpcyBwcmVwZW5kZWQgdG8gdGhlIGluZGl2aWR1YWxcbiAgICAvLyAgIGVudHJpZXMgaW4gdGhlIOKAnHNvdXJjZeKAnSBmaWVsZC5cbiAgICBzb3VyY2VVUkwgPSBzb3VyY2VSb290ICsgc291cmNlVVJMO1xuICB9XG5cbiAgLy8gSGlzdG9yaWNhbGx5LCBTb3VyY2VNYXBDb25zdW1lciBkaWQgbm90IHRha2UgdGhlIHNvdXJjZU1hcFVSTCBhc1xuICAvLyBhIHBhcmFtZXRlci4gIFRoaXMgbW9kZSBpcyBzdGlsbCBzb21ld2hhdCBzdXBwb3J0ZWQsIHdoaWNoIGlzIHdoeVxuICAvLyB0aGlzIGNvZGUgYmxvY2sgaXMgY29uZGl0aW9uYWwuICBIb3dldmVyLCBpdCdzIHByZWZlcmFibGUgdG8gcGFzc1xuICAvLyB0aGUgc291cmNlIG1hcCBVUkwgdG8gU291cmNlTWFwQ29uc3VtZXIsIHNvIHRoYXQgdGhpcyBmdW5jdGlvblxuICAvLyBjYW4gaW1wbGVtZW50IHRoZSBzb3VyY2UgVVJMIHJlc29sdXRpb24gYWxnb3JpdGhtIGFzIG91dGxpbmVkIGluXG4gIC8vIHRoZSBzcGVjLiAgVGhpcyBibG9jayBpcyBiYXNpY2FsbHkgdGhlIGVxdWl2YWxlbnQgb2Y6XG4gIC8vICAgIG5ldyBVUkwoc291cmNlVVJMLCBzb3VyY2VNYXBVUkwpLnRvU3RyaW5nKClcbiAgLy8gLi4uIGV4Y2VwdCBpdCBhdm9pZHMgdXNpbmcgVVJMLCB3aGljaCB3YXNuJ3QgYXZhaWxhYmxlIGluIHRoZVxuICAvLyBvbGRlciByZWxlYXNlcyBvZiBub2RlIHN0aWxsIHN1cHBvcnRlZCBieSB0aGlzIGxpYnJhcnkuXG4gIC8vXG4gIC8vIFRoZSBzcGVjIHNheXM6XG4gIC8vICAgSWYgdGhlIHNvdXJjZXMgYXJlIG5vdCBhYnNvbHV0ZSBVUkxzIGFmdGVyIHByZXBlbmRpbmcgb2YgdGhlXG4gIC8vICAg4oCcc291cmNlUm9vdOKAnSwgdGhlIHNvdXJjZXMgYXJlIHJlc29sdmVkIHJlbGF0aXZlIHRvIHRoZVxuICAvLyAgIFNvdXJjZU1hcCAobGlrZSByZXNvbHZpbmcgc2NyaXB0IHNyYyBpbiBhIGh0bWwgZG9jdW1lbnQpLlxuICBpZiAoc291cmNlTWFwVVJMKSB7XG4gICAgdmFyIHBhcnNlZCA9IHVybFBhcnNlKHNvdXJjZU1hcFVSTCk7XG4gICAgaWYgKCFwYXJzZWQpIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcihcInNvdXJjZU1hcFVSTCBjb3VsZCBub3QgYmUgcGFyc2VkXCIpO1xuICAgIH1cbiAgICBpZiAocGFyc2VkLnBhdGgpIHtcbiAgICAgIC8vIFN0cmlwIHRoZSBsYXN0IHBhdGggY29tcG9uZW50LCBidXQga2VlcCB0aGUgXCIvXCIuXG4gICAgICB2YXIgaW5kZXggPSBwYXJzZWQucGF0aC5sYXN0SW5kZXhPZignLycpO1xuICAgICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgICAgcGFyc2VkLnBhdGggPSBwYXJzZWQucGF0aC5zdWJzdHJpbmcoMCwgaW5kZXggKyAxKTtcbiAgICAgIH1cbiAgICB9XG4gICAgc291cmNlVVJMID0gam9pbih1cmxHZW5lcmF0ZShwYXJzZWQpLCBzb3VyY2VVUkwpO1xuICB9XG5cbiAgcmV0dXJuIG5vcm1hbGl6ZShzb3VyY2VVUkwpO1xufVxuZXhwb3J0cy5jb21wdXRlU291cmNlVVJMID0gY29tcHV0ZVNvdXJjZVVSTDtcblxuXG5cbi8vLy8vLy8vLy8vLy8vLy8vL1xuLy8gV0VCUEFDSyBGT09URVJcbi8vIC4vbGliL3V0aWwuanNcbi8vIG1vZHVsZSBpZCA9IDJcbi8vIG1vZHVsZSBjaHVua3MgPSAwIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xuXG52YXIgdXRpbCA9IHJlcXVpcmUoJy4vdXRpbCcpO1xudmFyIGJpbmFyeVNlYXJjaCA9IHJlcXVpcmUoJy4vYmluYXJ5LXNlYXJjaCcpO1xudmFyIEFycmF5U2V0ID0gcmVxdWlyZSgnLi9hcnJheS1zZXQnKS5BcnJheVNldDtcbnZhciBiYXNlNjRWTFEgPSByZXF1aXJlKCcuL2Jhc2U2NC12bHEnKTtcbnZhciBxdWlja1NvcnQgPSByZXF1aXJlKCcuL3F1aWNrLXNvcnQnKS5xdWlja1NvcnQ7XG5cbmZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyKGFTb3VyY2VNYXAsIGFTb3VyY2VNYXBVUkwpIHtcbiAgdmFyIHNvdXJjZU1hcCA9IGFTb3VyY2VNYXA7XG4gIGlmICh0eXBlb2YgYVNvdXJjZU1hcCA9PT0gJ3N0cmluZycpIHtcbiAgICBzb3VyY2VNYXAgPSB1dGlsLnBhcnNlU291cmNlTWFwSW5wdXQoYVNvdXJjZU1hcCk7XG4gIH1cblxuICByZXR1cm4gc291cmNlTWFwLnNlY3Rpb25zICE9IG51bGxcbiAgICA/IG5ldyBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIoc291cmNlTWFwLCBhU291cmNlTWFwVVJMKVxuICAgIDogbmV3IEJhc2ljU291cmNlTWFwQ29uc3VtZXIoc291cmNlTWFwLCBhU291cmNlTWFwVVJMKTtcbn1cblxuU291cmNlTWFwQ29uc3VtZXIuZnJvbVNvdXJjZU1hcCA9IGZ1bmN0aW9uKGFTb3VyY2VNYXAsIGFTb3VyY2VNYXBVUkwpIHtcbiAgcmV0dXJuIEJhc2ljU291cmNlTWFwQ29uc3VtZXIuZnJvbVNvdXJjZU1hcChhU291cmNlTWFwLCBhU291cmNlTWFwVVJMKTtcbn1cblxuLyoqXG4gKiBUaGUgdmVyc2lvbiBvZiB0aGUgc291cmNlIG1hcHBpbmcgc3BlYyB0aGF0IHdlIGFyZSBjb25zdW1pbmcuXG4gKi9cblNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbi8vIGBfX2dlbmVyYXRlZE1hcHBpbmdzYCBhbmQgYF9fb3JpZ2luYWxNYXBwaW5nc2AgYXJlIGFycmF5cyB0aGF0IGhvbGQgdGhlXG4vLyBwYXJzZWQgbWFwcGluZyBjb29yZGluYXRlcyBmcm9tIHRoZSBzb3VyY2UgbWFwJ3MgXCJtYXBwaW5nc1wiIGF0dHJpYnV0ZS4gVGhleVxuLy8gYXJlIGxhemlseSBpbnN0YW50aWF0ZWQsIGFjY2Vzc2VkIHZpYSB0aGUgYF9nZW5lcmF0ZWRNYXBwaW5nc2AgYW5kXG4vLyBgX29yaWdpbmFsTWFwcGluZ3NgIGdldHRlcnMgcmVzcGVjdGl2ZWx5LCBhbmQgd2Ugb25seSBwYXJzZSB0aGUgbWFwcGluZ3Ncbi8vIGFuZCBjcmVhdGUgdGhlc2UgYXJyYXlzIG9uY2UgcXVlcmllZCBmb3IgYSBzb3VyY2UgbG9jYXRpb24uIFdlIGp1bXAgdGhyb3VnaFxuLy8gdGhlc2UgaG9vcHMgYmVjYXVzZSB0aGVyZSBjYW4gYmUgbWFueSB0aG91c2FuZHMgb2YgbWFwcGluZ3MsIGFuZCBwYXJzaW5nXG4vLyB0aGVtIGlzIGV4cGVuc2l2ZSwgc28gd2Ugb25seSB3YW50IHRvIGRvIGl0IGlmIHdlIG11c3QuXG4vL1xuLy8gRWFjaCBvYmplY3QgaW4gdGhlIGFycmF5cyBpcyBvZiB0aGUgZm9ybTpcbi8vXG4vLyAgICAge1xuLy8gICAgICAgZ2VuZXJhdGVkTGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgY29kZSxcbi8vICAgICAgIGdlbmVyYXRlZENvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBjb2RlLFxuLy8gICAgICAgc291cmNlOiBUaGUgcGF0aCB0byB0aGUgb3JpZ2luYWwgc291cmNlIGZpbGUgdGhhdCBnZW5lcmF0ZWQgdGhpc1xuLy8gICAgICAgICAgICAgICBjaHVuayBvZiBjb2RlLFxuLy8gICAgICAgb3JpZ2luYWxMaW5lOiBUaGUgbGluZSBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZSB0aGF0XG4vLyAgICAgICAgICAgICAgICAgICAgIGNvcnJlc3BvbmRzIHRvIHRoaXMgY2h1bmsgb2YgZ2VuZXJhdGVkIGNvZGUsXG4vLyAgICAgICBvcmlnaW5hbENvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZSB0aGF0XG4vLyAgICAgICAgICAgICAgICAgICAgICAgY29ycmVzcG9uZHMgdG8gdGhpcyBjaHVuayBvZiBnZW5lcmF0ZWQgY29kZSxcbi8vICAgICAgIG5hbWU6IFRoZSBuYW1lIG9mIHRoZSBvcmlnaW5hbCBzeW1ib2wgd2hpY2ggZ2VuZXJhdGVkIHRoaXMgY2h1bmsgb2Zcbi8vICAgICAgICAgICAgIGNvZGUuXG4vLyAgICAgfVxuLy9cbi8vIEFsbCBwcm9wZXJ0aWVzIGV4Y2VwdCBmb3IgYGdlbmVyYXRlZExpbmVgIGFuZCBgZ2VuZXJhdGVkQ29sdW1uYCBjYW4gYmVcbi8vIGBudWxsYC5cbi8vXG4vLyBgX2dlbmVyYXRlZE1hcHBpbmdzYCBpcyBvcmRlcmVkIGJ5IHRoZSBnZW5lcmF0ZWQgcG9zaXRpb25zLlxuLy9cbi8vIGBfb3JpZ2luYWxNYXBwaW5nc2AgaXMgb3JkZXJlZCBieSB0aGUgb3JpZ2luYWwgcG9zaXRpb25zLlxuXG5Tb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX19nZW5lcmF0ZWRNYXBwaW5ncyA9IG51bGw7XG5PYmplY3QuZGVmaW5lUHJvcGVydHkoU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLCAnX2dlbmVyYXRlZE1hcHBpbmdzJywge1xuICBjb25maWd1cmFibGU6IHRydWUsXG4gIGVudW1lcmFibGU6IHRydWUsXG4gIGdldDogZnVuY3Rpb24gKCkge1xuICAgIGlmICghdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzKSB7XG4gICAgICB0aGlzLl9wYXJzZU1hcHBpbmdzKHRoaXMuX21hcHBpbmdzLCB0aGlzLnNvdXJjZVJvb3QpO1xuICAgIH1cblxuICAgIHJldHVybiB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3M7XG4gIH1cbn0pO1xuXG5Tb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX19vcmlnaW5hbE1hcHBpbmdzID0gbnVsbDtcbk9iamVjdC5kZWZpbmVQcm9wZXJ0eShTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUsICdfb3JpZ2luYWxNYXBwaW5ncycsIHtcbiAgY29uZmlndXJhYmxlOiB0cnVlLFxuICBlbnVtZXJhYmxlOiB0cnVlLFxuICBnZXQ6IGZ1bmN0aW9uICgpIHtcbiAgICBpZiAoIXRoaXMuX19vcmlnaW5hbE1hcHBpbmdzKSB7XG4gICAgICB0aGlzLl9wYXJzZU1hcHBpbmdzKHRoaXMuX21hcHBpbmdzLCB0aGlzLnNvdXJjZVJvb3QpO1xuICAgIH1cblxuICAgIHJldHVybiB0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncztcbiAgfVxufSk7XG5cblNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fY2hhcklzTWFwcGluZ1NlcGFyYXRvciA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2NoYXJJc01hcHBpbmdTZXBhcmF0b3IoYVN0ciwgaW5kZXgpIHtcbiAgICB2YXIgYyA9IGFTdHIuY2hhckF0KGluZGV4KTtcbiAgICByZXR1cm4gYyA9PT0gXCI7XCIgfHwgYyA9PT0gXCIsXCI7XG4gIH07XG5cbi8qKlxuICogUGFyc2UgdGhlIG1hcHBpbmdzIGluIGEgc3RyaW5nIGluIHRvIGEgZGF0YSBzdHJ1Y3R1cmUgd2hpY2ggd2UgY2FuIGVhc2lseVxuICogcXVlcnkgKHRoZSBvcmRlcmVkIGFycmF5cyBpbiB0aGUgYHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5nc2AgYW5kXG4gKiBgdGhpcy5fX29yaWdpbmFsTWFwcGluZ3NgIHByb3BlcnRpZXMpLlxuICovXG5Tb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9wYXJzZU1hcHBpbmdzKGFTdHIsIGFTb3VyY2VSb290KSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKFwiU3ViY2xhc3NlcyBtdXN0IGltcGxlbWVudCBfcGFyc2VNYXBwaW5nc1wiKTtcbiAgfTtcblxuU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSID0gMTtcblNvdXJjZU1hcENvbnN1bWVyLk9SSUdJTkFMX09SREVSID0gMjtcblxuU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQgPSAxO1xuU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQgPSAyO1xuXG4vKipcbiAqIEl0ZXJhdGUgb3ZlciBlYWNoIG1hcHBpbmcgYmV0d2VlbiBhbiBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4gYW5kIGFcbiAqIGdlbmVyYXRlZCBsaW5lL2NvbHVtbiBpbiB0aGlzIHNvdXJjZSBtYXAuXG4gKlxuICogQHBhcmFtIEZ1bmN0aW9uIGFDYWxsYmFja1xuICogICAgICAgIFRoZSBmdW5jdGlvbiB0aGF0IGlzIGNhbGxlZCB3aXRoIGVhY2ggbWFwcGluZy5cbiAqIEBwYXJhbSBPYmplY3QgYUNvbnRleHRcbiAqICAgICAgICBPcHRpb25hbC4gSWYgc3BlY2lmaWVkLCB0aGlzIG9iamVjdCB3aWxsIGJlIHRoZSB2YWx1ZSBvZiBgdGhpc2AgZXZlcnlcbiAqICAgICAgICB0aW1lIHRoYXQgYGFDYWxsYmFja2AgaXMgY2FsbGVkLlxuICogQHBhcmFtIGFPcmRlclxuICogICAgICAgIEVpdGhlciBgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSYCBvclxuICogICAgICAgIGBTb3VyY2VNYXBDb25zdW1lci5PUklHSU5BTF9PUkRFUmAuIFNwZWNpZmllcyB3aGV0aGVyIHlvdSB3YW50IHRvXG4gKiAgICAgICAgaXRlcmF0ZSBvdmVyIHRoZSBtYXBwaW5ncyBzb3J0ZWQgYnkgdGhlIGdlbmVyYXRlZCBmaWxlJ3MgbGluZS9jb2x1bW5cbiAqICAgICAgICBvcmRlciBvciB0aGUgb3JpZ2luYWwncyBzb3VyY2UvbGluZS9jb2x1bW4gb3JkZXIsIHJlc3BlY3RpdmVseS4gRGVmYXVsdHMgdG9cbiAqICAgICAgICBgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSYC5cbiAqL1xuU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmVhY2hNYXBwaW5nID1cbiAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfZWFjaE1hcHBpbmcoYUNhbGxiYWNrLCBhQ29udGV4dCwgYU9yZGVyKSB7XG4gICAgdmFyIGNvbnRleHQgPSBhQ29udGV4dCB8fCBudWxsO1xuICAgIHZhciBvcmRlciA9IGFPcmRlciB8fCBTb3VyY2VNYXBDb25zdW1lci5HRU5FUkFURURfT1JERVI7XG5cbiAgICB2YXIgbWFwcGluZ3M7XG4gICAgc3dpdGNoIChvcmRlcikge1xuICAgIGNhc2UgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSOlxuICAgICAgbWFwcGluZ3MgPSB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5ncztcbiAgICAgIGJyZWFrO1xuICAgIGNhc2UgU291cmNlTWFwQ29uc3VtZXIuT1JJR0lOQUxfT1JERVI6XG4gICAgICBtYXBwaW5ncyA9IHRoaXMuX29yaWdpbmFsTWFwcGluZ3M7XG4gICAgICBicmVhaztcbiAgICBkZWZhdWx0OlxuICAgICAgdGhyb3cgbmV3IEVycm9yKFwiVW5rbm93biBvcmRlciBvZiBpdGVyYXRpb24uXCIpO1xuICAgIH1cblxuICAgIHZhciBzb3VyY2VSb290ID0gdGhpcy5zb3VyY2VSb290O1xuICAgIG1hcHBpbmdzLm1hcChmdW5jdGlvbiAobWFwcGluZykge1xuICAgICAgdmFyIHNvdXJjZSA9IG1hcHBpbmcuc291cmNlID09PSBudWxsID8gbnVsbCA6IHRoaXMuX3NvdXJjZXMuYXQobWFwcGluZy5zb3VyY2UpO1xuICAgICAgc291cmNlID0gdXRpbC5jb21wdXRlU291cmNlVVJMKHNvdXJjZVJvb3QsIHNvdXJjZSwgdGhpcy5fc291cmNlTWFwVVJMKTtcbiAgICAgIHJldHVybiB7XG4gICAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgICBnZW5lcmF0ZWRMaW5lOiBtYXBwaW5nLmdlbmVyYXRlZExpbmUsXG4gICAgICAgIGdlbmVyYXRlZENvbHVtbjogbWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4sXG4gICAgICAgIG9yaWdpbmFsTGluZTogbWFwcGluZy5vcmlnaW5hbExpbmUsXG4gICAgICAgIG9yaWdpbmFsQ29sdW1uOiBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uLFxuICAgICAgICBuYW1lOiBtYXBwaW5nLm5hbWUgPT09IG51bGwgPyBudWxsIDogdGhpcy5fbmFtZXMuYXQobWFwcGluZy5uYW1lKVxuICAgICAgfTtcbiAgICB9LCB0aGlzKS5mb3JFYWNoKGFDYWxsYmFjaywgY29udGV4dCk7XG4gIH07XG5cbi8qKlxuICogUmV0dXJucyBhbGwgZ2VuZXJhdGVkIGxpbmUgYW5kIGNvbHVtbiBpbmZvcm1hdGlvbiBmb3IgdGhlIG9yaWdpbmFsIHNvdXJjZSxcbiAqIGxpbmUsIGFuZCBjb2x1bW4gcHJvdmlkZWQuIElmIG5vIGNvbHVtbiBpcyBwcm92aWRlZCwgcmV0dXJucyBhbGwgbWFwcGluZ3NcbiAqIGNvcnJlc3BvbmRpbmcgdG8gYSBlaXRoZXIgdGhlIGxpbmUgd2UgYXJlIHNlYXJjaGluZyBmb3Igb3IgdGhlIG5leHRcbiAqIGNsb3Nlc3QgbGluZSB0aGF0IGhhcyBhbnkgbWFwcGluZ3MuIE90aGVyd2lzZSwgcmV0dXJucyBhbGwgbWFwcGluZ3NcbiAqIGNvcnJlc3BvbmRpbmcgdG8gdGhlIGdpdmVuIGxpbmUgYW5kIGVpdGhlciB0aGUgY29sdW1uIHdlIGFyZSBzZWFyY2hpbmcgZm9yXG4gKiBvciB0aGUgbmV4dCBjbG9zZXN0IGNvbHVtbiB0aGF0IGhhcyBhbnkgb2Zmc2V0cy5cbiAqXG4gKiBUaGUgb25seSBhcmd1bWVudCBpcyBhbiBvYmplY3Qgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIHNvdXJjZTogVGhlIGZpbGVuYW1lIG9mIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuICBUaGUgbGluZSBudW1iZXIgaXMgMS1iYXNlZC5cbiAqICAgLSBjb2x1bW46IE9wdGlvbmFsLiB0aGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICogICAgVGhlIGNvbHVtbiBudW1iZXIgaXMgMC1iYXNlZC5cbiAqXG4gKiBhbmQgYW4gYXJyYXkgb2Ygb2JqZWN0cyBpcyByZXR1cm5lZCwgZWFjaCB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLCBvciBudWxsLiAgVGhlXG4gKiAgICBsaW5lIG51bWJlciBpcyAxLWJhc2VkLlxuICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UsIG9yIG51bGwuXG4gKiAgICBUaGUgY29sdW1uIG51bWJlciBpcyAwLWJhc2VkLlxuICovXG5Tb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yID1cbiAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKGFBcmdzKSB7XG4gICAgdmFyIGxpbmUgPSB1dGlsLmdldEFyZyhhQXJncywgJ2xpbmUnKTtcblxuICAgIC8vIFdoZW4gdGhlcmUgaXMgbm8gZXhhY3QgbWF0Y2gsIEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLl9maW5kTWFwcGluZ1xuICAgIC8vIHJldHVybnMgdGhlIGluZGV4IG9mIHRoZSBjbG9zZXN0IG1hcHBpbmcgbGVzcyB0aGFuIHRoZSBuZWVkbGUuIEJ5XG4gICAgLy8gc2V0dGluZyBuZWVkbGUub3JpZ2luYWxDb2x1bW4gdG8gMCwgd2UgdGh1cyBmaW5kIHRoZSBsYXN0IG1hcHBpbmcgZm9yXG4gICAgLy8gdGhlIGdpdmVuIGxpbmUsIHByb3ZpZGVkIHN1Y2ggYSBtYXBwaW5nIGV4aXN0cy5cbiAgICB2YXIgbmVlZGxlID0ge1xuICAgICAgc291cmNlOiB1dGlsLmdldEFyZyhhQXJncywgJ3NvdXJjZScpLFxuICAgICAgb3JpZ2luYWxMaW5lOiBsaW5lLFxuICAgICAgb3JpZ2luYWxDb2x1bW46IHV0aWwuZ2V0QXJnKGFBcmdzLCAnY29sdW1uJywgMClcbiAgICB9O1xuXG4gICAgbmVlZGxlLnNvdXJjZSA9IHRoaXMuX2ZpbmRTb3VyY2VJbmRleChuZWVkbGUuc291cmNlKTtcbiAgICBpZiAobmVlZGxlLnNvdXJjZSA8IDApIHtcbiAgICAgIHJldHVybiBbXTtcbiAgICB9XG5cbiAgICB2YXIgbWFwcGluZ3MgPSBbXTtcblxuICAgIHZhciBpbmRleCA9IHRoaXMuX2ZpbmRNYXBwaW5nKG5lZWRsZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIFwib3JpZ2luYWxMaW5lXCIsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgXCJvcmlnaW5hbENvbHVtblwiLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIHV0aWwuY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgYmluYXJ5U2VhcmNoLkxFQVNUX1VQUEVSX0JPVU5EKTtcbiAgICBpZiAoaW5kZXggPj0gMCkge1xuICAgICAgdmFyIG1hcHBpbmcgPSB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgaWYgKGFBcmdzLmNvbHVtbiA9PT0gdW5kZWZpbmVkKSB7XG4gICAgICAgIHZhciBvcmlnaW5hbExpbmUgPSBtYXBwaW5nLm9yaWdpbmFsTGluZTtcblxuICAgICAgICAvLyBJdGVyYXRlIHVudGlsIGVpdGhlciB3ZSBydW4gb3V0IG9mIG1hcHBpbmdzLCBvciB3ZSBydW4gaW50b1xuICAgICAgICAvLyBhIG1hcHBpbmcgZm9yIGEgZGlmZmVyZW50IGxpbmUgdGhhbiB0aGUgb25lIHdlIGZvdW5kLiBTaW5jZVxuICAgICAgICAvLyBtYXBwaW5ncyBhcmUgc29ydGVkLCB0aGlzIGlzIGd1YXJhbnRlZWQgdG8gZmluZCBhbGwgbWFwcGluZ3MgZm9yXG4gICAgICAgIC8vIHRoZSBsaW5lIHdlIGZvdW5kLlxuICAgICAgICB3aGlsZSAobWFwcGluZyAmJiBtYXBwaW5nLm9yaWdpbmFsTGluZSA9PT0gb3JpZ2luYWxMaW5lKSB7XG4gICAgICAgICAgbWFwcGluZ3MucHVzaCh7XG4gICAgICAgICAgICBsaW5lOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkTGluZScsIG51bGwpLFxuICAgICAgICAgICAgY29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkQ29sdW1uJywgbnVsbCksXG4gICAgICAgICAgICBsYXN0Q29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnbGFzdEdlbmVyYXRlZENvbHVtbicsIG51bGwpXG4gICAgICAgICAgfSk7XG5cbiAgICAgICAgICBtYXBwaW5nID0gdGhpcy5fb3JpZ2luYWxNYXBwaW5nc1srK2luZGV4XTtcbiAgICAgICAgfVxuICAgICAgfSBlbHNlIHtcbiAgICAgICAgdmFyIG9yaWdpbmFsQ29sdW1uID0gbWFwcGluZy5vcmlnaW5hbENvbHVtbjtcblxuICAgICAgICAvLyBJdGVyYXRlIHVudGlsIGVpdGhlciB3ZSBydW4gb3V0IG9mIG1hcHBpbmdzLCBvciB3ZSBydW4gaW50b1xuICAgICAgICAvLyBhIG1hcHBpbmcgZm9yIGEgZGlmZmVyZW50IGxpbmUgdGhhbiB0aGUgb25lIHdlIHdlcmUgc2VhcmNoaW5nIGZvci5cbiAgICAgICAgLy8gU2luY2UgbWFwcGluZ3MgYXJlIHNvcnRlZCwgdGhpcyBpcyBndWFyYW50ZWVkIHRvIGZpbmQgYWxsIG1hcHBpbmdzIGZvclxuICAgICAgICAvLyB0aGUgbGluZSB3ZSBhcmUgc2VhcmNoaW5nIGZvci5cbiAgICAgICAgd2hpbGUgKG1hcHBpbmcgJiZcbiAgICAgICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxMaW5lID09PSBsaW5lICYmXG4gICAgICAgICAgICAgICBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uID09IG9yaWdpbmFsQ29sdW1uKSB7XG4gICAgICAgICAgbWFwcGluZ3MucHVzaCh7XG4gICAgICAgICAgICBsaW5lOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkTGluZScsIG51bGwpLFxuICAgICAgICAgICAgY29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkQ29sdW1uJywgbnVsbCksXG4gICAgICAgICAgICBsYXN0Q29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnbGFzdEdlbmVyYXRlZENvbHVtbicsIG51bGwpXG4gICAgICAgICAgfSk7XG5cbiAgICAgICAgICBtYXBwaW5nID0gdGhpcy5fb3JpZ2luYWxNYXBwaW5nc1srK2luZGV4XTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH1cblxuICAgIHJldHVybiBtYXBwaW5ncztcbiAgfTtcblxuZXhwb3J0cy5Tb3VyY2VNYXBDb25zdW1lciA9IFNvdXJjZU1hcENvbnN1bWVyO1xuXG4vKipcbiAqIEEgQmFzaWNTb3VyY2VNYXBDb25zdW1lciBpbnN0YW5jZSByZXByZXNlbnRzIGEgcGFyc2VkIHNvdXJjZSBtYXAgd2hpY2ggd2UgY2FuXG4gKiBxdWVyeSBmb3IgaW5mb3JtYXRpb24gYWJvdXQgdGhlIG9yaWdpbmFsIGZpbGUgcG9zaXRpb25zIGJ5IGdpdmluZyBpdCBhIGZpbGVcbiAqIHBvc2l0aW9uIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLlxuICpcbiAqIFRoZSBmaXJzdCBwYXJhbWV0ZXIgaXMgdGhlIHJhdyBzb3VyY2UgbWFwIChlaXRoZXIgYXMgYSBKU09OIHN0cmluZywgb3JcbiAqIGFscmVhZHkgcGFyc2VkIHRvIGFuIG9iamVjdCkuIEFjY29yZGluZyB0byB0aGUgc3BlYywgc291cmNlIG1hcHMgaGF2ZSB0aGVcbiAqIGZvbGxvd2luZyBhdHRyaWJ1dGVzOlxuICpcbiAqICAgLSB2ZXJzaW9uOiBXaGljaCB2ZXJzaW9uIG9mIHRoZSBzb3VyY2UgbWFwIHNwZWMgdGhpcyBtYXAgaXMgZm9sbG93aW5nLlxuICogICAtIHNvdXJjZXM6IEFuIGFycmF5IG9mIFVSTHMgdG8gdGhlIG9yaWdpbmFsIHNvdXJjZSBmaWxlcy5cbiAqICAgLSBuYW1lczogQW4gYXJyYXkgb2YgaWRlbnRpZmllcnMgd2hpY2ggY2FuIGJlIHJlZmVycmVuY2VkIGJ5IGluZGl2aWR1YWwgbWFwcGluZ3MuXG4gKiAgIC0gc291cmNlUm9vdDogT3B0aW9uYWwuIFRoZSBVUkwgcm9vdCBmcm9tIHdoaWNoIGFsbCBzb3VyY2VzIGFyZSByZWxhdGl2ZS5cbiAqICAgLSBzb3VyY2VzQ29udGVudDogT3B0aW9uYWwuIEFuIGFycmF5IG9mIGNvbnRlbnRzIG9mIHRoZSBvcmlnaW5hbCBzb3VyY2UgZmlsZXMuXG4gKiAgIC0gbWFwcGluZ3M6IEEgc3RyaW5nIG9mIGJhc2U2NCBWTFFzIHdoaWNoIGNvbnRhaW4gdGhlIGFjdHVhbCBtYXBwaW5ncy5cbiAqICAgLSBmaWxlOiBPcHRpb25hbC4gVGhlIGdlbmVyYXRlZCBmaWxlIHRoaXMgc291cmNlIG1hcCBpcyBhc3NvY2lhdGVkIHdpdGguXG4gKlxuICogSGVyZSBpcyBhbiBleGFtcGxlIHNvdXJjZSBtYXAsIHRha2VuIGZyb20gdGhlIHNvdXJjZSBtYXAgc3BlY1swXTpcbiAqXG4gKiAgICAge1xuICogICAgICAgdmVyc2lvbiA6IDMsXG4gKiAgICAgICBmaWxlOiBcIm91dC5qc1wiLFxuICogICAgICAgc291cmNlUm9vdCA6IFwiXCIsXG4gKiAgICAgICBzb3VyY2VzOiBbXCJmb28uanNcIiwgXCJiYXIuanNcIl0sXG4gKiAgICAgICBuYW1lczogW1wic3JjXCIsIFwibWFwc1wiLCBcImFyZVwiLCBcImZ1blwiXSxcbiAqICAgICAgIG1hcHBpbmdzOiBcIkFBLEFCOztBQkNERTtcIlxuICogICAgIH1cbiAqXG4gKiBUaGUgc2Vjb25kIHBhcmFtZXRlciwgaWYgZ2l2ZW4sIGlzIGEgc3RyaW5nIHdob3NlIHZhbHVlIGlzIHRoZSBVUkxcbiAqIGF0IHdoaWNoIHRoZSBzb3VyY2UgbWFwIHdhcyBmb3VuZC4gIFRoaXMgVVJMIGlzIHVzZWQgdG8gY29tcHV0ZSB0aGVcbiAqIHNvdXJjZXMgYXJyYXkuXG4gKlxuICogWzBdOiBodHRwczovL2RvY3MuZ29vZ2xlLmNvbS9kb2N1bWVudC9kLzFVMVJHQWVoUXdSeXBVVG92RjFLUmxwaU9GemUwYi1fMmdjNmZBSDBLWTBrL2VkaXQ/cGxpPTEjXG4gKi9cbmZ1bmN0aW9uIEJhc2ljU291cmNlTWFwQ29uc3VtZXIoYVNvdXJjZU1hcCwgYVNvdXJjZU1hcFVSTCkge1xuICB2YXIgc291cmNlTWFwID0gYVNvdXJjZU1hcDtcbiAgaWYgKHR5cGVvZiBhU291cmNlTWFwID09PSAnc3RyaW5nJykge1xuICAgIHNvdXJjZU1hcCA9IHV0aWwucGFyc2VTb3VyY2VNYXBJbnB1dChhU291cmNlTWFwKTtcbiAgfVxuXG4gIHZhciB2ZXJzaW9uID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAndmVyc2lvbicpO1xuICB2YXIgc291cmNlcyA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ3NvdXJjZXMnKTtcbiAgLy8gU2FzcyAzLjMgbGVhdmVzIG91dCB0aGUgJ25hbWVzJyBhcnJheSwgc28gd2UgZGV2aWF0ZSBmcm9tIHRoZSBzcGVjICh3aGljaFxuICAvLyByZXF1aXJlcyB0aGUgYXJyYXkpIHRvIHBsYXkgbmljZSBoZXJlLlxuICB2YXIgbmFtZXMgPSB1dGlsLmdldEFyZyhzb3VyY2VNYXAsICduYW1lcycsIFtdKTtcbiAgdmFyIHNvdXJjZVJvb3QgPSB1dGlsLmdldEFyZyhzb3VyY2VNYXAsICdzb3VyY2VSb290JywgbnVsbCk7XG4gIHZhciBzb3VyY2VzQ29udGVudCA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ3NvdXJjZXNDb250ZW50JywgbnVsbCk7XG4gIHZhciBtYXBwaW5ncyA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ21hcHBpbmdzJyk7XG4gIHZhciBmaWxlID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnZmlsZScsIG51bGwpO1xuXG4gIC8vIE9uY2UgYWdhaW4sIFNhc3MgZGV2aWF0ZXMgZnJvbSB0aGUgc3BlYyBhbmQgc3VwcGxpZXMgdGhlIHZlcnNpb24gYXMgYVxuICAvLyBzdHJpbmcgcmF0aGVyIHRoYW4gYSBudW1iZXIsIHNvIHdlIHVzZSBsb29zZSBlcXVhbGl0eSBjaGVja2luZyBoZXJlLlxuICBpZiAodmVyc2lvbiAhPSB0aGlzLl92ZXJzaW9uKSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKCdVbnN1cHBvcnRlZCB2ZXJzaW9uOiAnICsgdmVyc2lvbik7XG4gIH1cblxuICBpZiAoc291cmNlUm9vdCkge1xuICAgIHNvdXJjZVJvb3QgPSB1dGlsLm5vcm1hbGl6ZShzb3VyY2VSb290KTtcbiAgfVxuXG4gIHNvdXJjZXMgPSBzb3VyY2VzXG4gICAgLm1hcChTdHJpbmcpXG4gICAgLy8gU29tZSBzb3VyY2UgbWFwcyBwcm9kdWNlIHJlbGF0aXZlIHNvdXJjZSBwYXRocyBsaWtlIFwiLi9mb28uanNcIiBpbnN0ZWFkIG9mXG4gICAgLy8gXCJmb28uanNcIi4gIE5vcm1hbGl6ZSB0aGVzZSBmaXJzdCBzbyB0aGF0IGZ1dHVyZSBjb21wYXJpc29ucyB3aWxsIHN1Y2NlZWQuXG4gICAgLy8gU2VlIGJ1Z3ppbC5sYS8xMDkwNzY4LlxuICAgIC5tYXAodXRpbC5ub3JtYWxpemUpXG4gICAgLy8gQWx3YXlzIGVuc3VyZSB0aGF0IGFic29sdXRlIHNvdXJjZXMgYXJlIGludGVybmFsbHkgc3RvcmVkIHJlbGF0aXZlIHRvXG4gICAgLy8gdGhlIHNvdXJjZSByb290LCBpZiB0aGUgc291cmNlIHJvb3QgaXMgYWJzb2x1dGUuIE5vdCBkb2luZyB0aGlzIHdvdWxkXG4gICAgLy8gYmUgcGFydGljdWxhcmx5IHByb2JsZW1hdGljIHdoZW4gdGhlIHNvdXJjZSByb290IGlzIGEgcHJlZml4IG9mIHRoZVxuICAgIC8vIHNvdXJjZSAodmFsaWQsIGJ1dCB3aHk/PykuIFNlZSBnaXRodWIgaXNzdWUgIzE5OSBhbmQgYnVnemlsLmxhLzExODg5ODIuXG4gICAgLm1hcChmdW5jdGlvbiAoc291cmNlKSB7XG4gICAgICByZXR1cm4gc291cmNlUm9vdCAmJiB1dGlsLmlzQWJzb2x1dGUoc291cmNlUm9vdCkgJiYgdXRpbC5pc0Fic29sdXRlKHNvdXJjZSlcbiAgICAgICAgPyB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIHNvdXJjZSlcbiAgICAgICAgOiBzb3VyY2U7XG4gICAgfSk7XG5cbiAgLy8gUGFzcyBgdHJ1ZWAgYmVsb3cgdG8gYWxsb3cgZHVwbGljYXRlIG5hbWVzIGFuZCBzb3VyY2VzLiBXaGlsZSBzb3VyY2UgbWFwc1xuICAvLyBhcmUgaW50ZW5kZWQgdG8gYmUgY29tcHJlc3NlZCBhbmQgZGVkdXBsaWNhdGVkLCB0aGUgVHlwZVNjcmlwdCBjb21waWxlclxuICAvLyBzb21ldGltZXMgZ2VuZXJhdGVzIHNvdXJjZSBtYXBzIHdpdGggZHVwbGljYXRlcyBpbiB0aGVtLiBTZWUgR2l0aHViIGlzc3VlXG4gIC8vICM3MiBhbmQgYnVnemlsLmxhLzg4OTQ5Mi5cbiAgdGhpcy5fbmFtZXMgPSBBcnJheVNldC5mcm9tQXJyYXkobmFtZXMubWFwKFN0cmluZyksIHRydWUpO1xuICB0aGlzLl9zb3VyY2VzID0gQXJyYXlTZXQuZnJvbUFycmF5KHNvdXJjZXMsIHRydWUpO1xuXG4gIHRoaXMuX2Fic29sdXRlU291cmNlcyA9IHRoaXMuX3NvdXJjZXMudG9BcnJheSgpLm1hcChmdW5jdGlvbiAocykge1xuICAgIHJldHVybiB1dGlsLmNvbXB1dGVTb3VyY2VVUkwoc291cmNlUm9vdCwgcywgYVNvdXJjZU1hcFVSTCk7XG4gIH0pO1xuXG4gIHRoaXMuc291cmNlUm9vdCA9IHNvdXJjZVJvb3Q7XG4gIHRoaXMuc291cmNlc0NvbnRlbnQgPSBzb3VyY2VzQ29udGVudDtcbiAgdGhpcy5fbWFwcGluZ3MgPSBtYXBwaW5ncztcbiAgdGhpcy5fc291cmNlTWFwVVJMID0gYVNvdXJjZU1hcFVSTDtcbiAgdGhpcy5maWxlID0gZmlsZTtcbn1cblxuQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUgPSBPYmplY3QuY3JlYXRlKFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSk7XG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5jb25zdW1lciA9IFNvdXJjZU1hcENvbnN1bWVyO1xuXG4vKipcbiAqIFV0aWxpdHkgZnVuY3Rpb24gdG8gZmluZCB0aGUgaW5kZXggb2YgYSBzb3VyY2UuICBSZXR1cm5zIC0xIGlmIG5vdFxuICogZm91bmQuXG4gKi9cbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLl9maW5kU291cmNlSW5kZXggPSBmdW5jdGlvbihhU291cmNlKSB7XG4gIHZhciByZWxhdGl2ZVNvdXJjZSA9IGFTb3VyY2U7XG4gIGlmICh0aGlzLnNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgIHJlbGF0aXZlU291cmNlID0gdXRpbC5yZWxhdGl2ZSh0aGlzLnNvdXJjZVJvb3QsIHJlbGF0aXZlU291cmNlKTtcbiAgfVxuXG4gIGlmICh0aGlzLl9zb3VyY2VzLmhhcyhyZWxhdGl2ZVNvdXJjZSkpIHtcbiAgICByZXR1cm4gdGhpcy5fc291cmNlcy5pbmRleE9mKHJlbGF0aXZlU291cmNlKTtcbiAgfVxuXG4gIC8vIE1heWJlIGFTb3VyY2UgaXMgYW4gYWJzb2x1dGUgVVJMIGFzIHJldHVybmVkIGJ5IHxzb3VyY2VzfC4gIEluXG4gIC8vIHRoaXMgY2FzZSB3ZSBjYW4ndCBzaW1wbHkgdW5kbyB0aGUgdHJhbnNmb3JtLlxuICB2YXIgaTtcbiAgZm9yIChpID0gMDsgaSA8IHRoaXMuX2Fic29sdXRlU291cmNlcy5sZW5ndGg7ICsraSkge1xuICAgIGlmICh0aGlzLl9hYnNvbHV0ZVNvdXJjZXNbaV0gPT0gYVNvdXJjZSkge1xuICAgICAgcmV0dXJuIGk7XG4gICAgfVxuICB9XG5cbiAgcmV0dXJuIC0xO1xufTtcblxuLyoqXG4gKiBDcmVhdGUgYSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyIGZyb20gYSBTb3VyY2VNYXBHZW5lcmF0b3IuXG4gKlxuICogQHBhcmFtIFNvdXJjZU1hcEdlbmVyYXRvciBhU291cmNlTWFwXG4gKiAgICAgICAgVGhlIHNvdXJjZSBtYXAgdGhhdCB3aWxsIGJlIGNvbnN1bWVkLlxuICogQHBhcmFtIFN0cmluZyBhU291cmNlTWFwVVJMXG4gKiAgICAgICAgVGhlIFVSTCBhdCB3aGljaCB0aGUgc291cmNlIG1hcCBjYW4gYmUgZm91bmQgKG9wdGlvbmFsKVxuICogQHJldHVybnMgQmFzaWNTb3VyY2VNYXBDb25zdW1lclxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9mcm9tU291cmNlTWFwKGFTb3VyY2VNYXAsIGFTb3VyY2VNYXBVUkwpIHtcbiAgICB2YXIgc21jID0gT2JqZWN0LmNyZWF0ZShCYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSk7XG5cbiAgICB2YXIgbmFtZXMgPSBzbWMuX25hbWVzID0gQXJyYXlTZXQuZnJvbUFycmF5KGFTb3VyY2VNYXAuX25hbWVzLnRvQXJyYXkoKSwgdHJ1ZSk7XG4gICAgdmFyIHNvdXJjZXMgPSBzbWMuX3NvdXJjZXMgPSBBcnJheVNldC5mcm9tQXJyYXkoYVNvdXJjZU1hcC5fc291cmNlcy50b0FycmF5KCksIHRydWUpO1xuICAgIHNtYy5zb3VyY2VSb290ID0gYVNvdXJjZU1hcC5fc291cmNlUm9vdDtcbiAgICBzbWMuc291cmNlc0NvbnRlbnQgPSBhU291cmNlTWFwLl9nZW5lcmF0ZVNvdXJjZXNDb250ZW50KHNtYy5fc291cmNlcy50b0FycmF5KCksXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBzbWMuc291cmNlUm9vdCk7XG4gICAgc21jLmZpbGUgPSBhU291cmNlTWFwLl9maWxlO1xuICAgIHNtYy5fc291cmNlTWFwVVJMID0gYVNvdXJjZU1hcFVSTDtcbiAgICBzbWMuX2Fic29sdXRlU291cmNlcyA9IHNtYy5fc291cmNlcy50b0FycmF5KCkubWFwKGZ1bmN0aW9uIChzKSB7XG4gICAgICByZXR1cm4gdXRpbC5jb21wdXRlU291cmNlVVJMKHNtYy5zb3VyY2VSb290LCBzLCBhU291cmNlTWFwVVJMKTtcbiAgICB9KTtcblxuICAgIC8vIEJlY2F1c2Ugd2UgYXJlIG1vZGlmeWluZyB0aGUgZW50cmllcyAoYnkgY29udmVydGluZyBzdHJpbmcgc291cmNlcyBhbmRcbiAgICAvLyBuYW1lcyB0byBpbmRpY2VzIGludG8gdGhlIHNvdXJjZXMgYW5kIG5hbWVzIEFycmF5U2V0cyksIHdlIGhhdmUgdG8gbWFrZVxuICAgIC8vIGEgY29weSBvZiB0aGUgZW50cnkgb3IgZWxzZSBiYWQgdGhpbmdzIGhhcHBlbi4gU2hhcmVkIG11dGFibGUgc3RhdGVcbiAgICAvLyBzdHJpa2VzIGFnYWluISBTZWUgZ2l0aHViIGlzc3VlICMxOTEuXG5cbiAgICB2YXIgZ2VuZXJhdGVkTWFwcGluZ3MgPSBhU291cmNlTWFwLl9tYXBwaW5ncy50b0FycmF5KCkuc2xpY2UoKTtcbiAgICB2YXIgZGVzdEdlbmVyYXRlZE1hcHBpbmdzID0gc21jLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBbXTtcbiAgICB2YXIgZGVzdE9yaWdpbmFsTWFwcGluZ3MgPSBzbWMuX19vcmlnaW5hbE1hcHBpbmdzID0gW107XG5cbiAgICBmb3IgKHZhciBpID0gMCwgbGVuZ3RoID0gZ2VuZXJhdGVkTWFwcGluZ3MubGVuZ3RoOyBpIDwgbGVuZ3RoOyBpKyspIHtcbiAgICAgIHZhciBzcmNNYXBwaW5nID0gZ2VuZXJhdGVkTWFwcGluZ3NbaV07XG4gICAgICB2YXIgZGVzdE1hcHBpbmcgPSBuZXcgTWFwcGluZztcbiAgICAgIGRlc3RNYXBwaW5nLmdlbmVyYXRlZExpbmUgPSBzcmNNYXBwaW5nLmdlbmVyYXRlZExpbmU7XG4gICAgICBkZXN0TWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4gPSBzcmNNYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgaWYgKHNyY01hcHBpbmcuc291cmNlKSB7XG4gICAgICAgIGRlc3RNYXBwaW5nLnNvdXJjZSA9IHNvdXJjZXMuaW5kZXhPZihzcmNNYXBwaW5nLnNvdXJjZSk7XG4gICAgICAgIGRlc3RNYXBwaW5nLm9yaWdpbmFsTGluZSA9IHNyY01hcHBpbmcub3JpZ2luYWxMaW5lO1xuICAgICAgICBkZXN0TWFwcGluZy5vcmlnaW5hbENvbHVtbiA9IHNyY01hcHBpbmcub3JpZ2luYWxDb2x1bW47XG5cbiAgICAgICAgaWYgKHNyY01hcHBpbmcubmFtZSkge1xuICAgICAgICAgIGRlc3RNYXBwaW5nLm5hbWUgPSBuYW1lcy5pbmRleE9mKHNyY01hcHBpbmcubmFtZSk7XG4gICAgICAgIH1cblxuICAgICAgICBkZXN0T3JpZ2luYWxNYXBwaW5ncy5wdXNoKGRlc3RNYXBwaW5nKTtcbiAgICAgIH1cblxuICAgICAgZGVzdEdlbmVyYXRlZE1hcHBpbmdzLnB1c2goZGVzdE1hcHBpbmcpO1xuICAgIH1cblxuICAgIHF1aWNrU29ydChzbWMuX19vcmlnaW5hbE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zKTtcblxuICAgIHJldHVybiBzbWM7XG4gIH07XG5cbi8qKlxuICogVGhlIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXBwaW5nIHNwZWMgdGhhdCB3ZSBhcmUgY29uc3VtaW5nLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbi8qKlxuICogVGhlIGxpc3Qgb2Ygb3JpZ2luYWwgc291cmNlcy5cbiAqL1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLCAnc291cmNlcycsIHtcbiAgZ2V0OiBmdW5jdGlvbiAoKSB7XG4gICAgcmV0dXJuIHRoaXMuX2Fic29sdXRlU291cmNlcy5zbGljZSgpO1xuICB9XG59KTtcblxuLyoqXG4gKiBQcm92aWRlIHRoZSBKSVQgd2l0aCBhIG5pY2Ugc2hhcGUgLyBoaWRkZW4gY2xhc3MuXG4gKi9cbmZ1bmN0aW9uIE1hcHBpbmcoKSB7XG4gIHRoaXMuZ2VuZXJhdGVkTGluZSA9IDA7XG4gIHRoaXMuZ2VuZXJhdGVkQ29sdW1uID0gMDtcbiAgdGhpcy5zb3VyY2UgPSBudWxsO1xuICB0aGlzLm9yaWdpbmFsTGluZSA9IG51bGw7XG4gIHRoaXMub3JpZ2luYWxDb2x1bW4gPSBudWxsO1xuICB0aGlzLm5hbWUgPSBudWxsO1xufVxuXG4vKipcbiAqIFBhcnNlIHRoZSBtYXBwaW5ncyBpbiBhIHN0cmluZyBpbiB0byBhIGRhdGEgc3RydWN0dXJlIHdoaWNoIHdlIGNhbiBlYXNpbHlcbiAqIHF1ZXJ5ICh0aGUgb3JkZXJlZCBhcnJheXMgaW4gdGhlIGB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3NgIGFuZFxuICogYHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzYCBwcm9wZXJ0aWVzKS5cbiAqL1xuQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9wYXJzZU1hcHBpbmdzKGFTdHIsIGFTb3VyY2VSb290KSB7XG4gICAgdmFyIGdlbmVyYXRlZExpbmUgPSAxO1xuICAgIHZhciBwcmV2aW91c0dlbmVyYXRlZENvbHVtbiA9IDA7XG4gICAgdmFyIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gMDtcbiAgICB2YXIgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IDA7XG4gICAgdmFyIHByZXZpb3VzU291cmNlID0gMDtcbiAgICB2YXIgcHJldmlvdXNOYW1lID0gMDtcbiAgICB2YXIgbGVuZ3RoID0gYVN0ci5sZW5ndGg7XG4gICAgdmFyIGluZGV4ID0gMDtcbiAgICB2YXIgY2FjaGVkU2VnbWVudHMgPSB7fTtcbiAgICB2YXIgdGVtcCA9IHt9O1xuICAgIHZhciBvcmlnaW5hbE1hcHBpbmdzID0gW107XG4gICAgdmFyIGdlbmVyYXRlZE1hcHBpbmdzID0gW107XG4gICAgdmFyIG1hcHBpbmcsIHN0ciwgc2VnbWVudCwgZW5kLCB2YWx1ZTtcblxuICAgIHdoaWxlIChpbmRleCA8IGxlbmd0aCkge1xuICAgICAgaWYgKGFTdHIuY2hhckF0KGluZGV4KSA9PT0gJzsnKSB7XG4gICAgICAgIGdlbmVyYXRlZExpbmUrKztcbiAgICAgICAgaW5kZXgrKztcbiAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSAwO1xuICAgICAgfVxuICAgICAgZWxzZSBpZiAoYVN0ci5jaGFyQXQoaW5kZXgpID09PSAnLCcpIHtcbiAgICAgICAgaW5kZXgrKztcbiAgICAgIH1cbiAgICAgIGVsc2Uge1xuICAgICAgICBtYXBwaW5nID0gbmV3IE1hcHBpbmcoKTtcbiAgICAgICAgbWFwcGluZy5nZW5lcmF0ZWRMaW5lID0gZ2VuZXJhdGVkTGluZTtcblxuICAgICAgICAvLyBCZWNhdXNlIGVhY2ggb2Zmc2V0IGlzIGVuY29kZWQgcmVsYXRpdmUgdG8gdGhlIHByZXZpb3VzIG9uZSxcbiAgICAgICAgLy8gbWFueSBzZWdtZW50cyBvZnRlbiBoYXZlIHRoZSBzYW1lIGVuY29kaW5nLiBXZSBjYW4gZXhwbG9pdCB0aGlzXG4gICAgICAgIC8vIGZhY3QgYnkgY2FjaGluZyB0aGUgcGFyc2VkIHZhcmlhYmxlIGxlbmd0aCBmaWVsZHMgb2YgZWFjaCBzZWdtZW50LFxuICAgICAgICAvLyBhbGxvd2luZyB1cyB0byBhdm9pZCBhIHNlY29uZCBwYXJzZSBpZiB3ZSBlbmNvdW50ZXIgdGhlIHNhbWVcbiAgICAgICAgLy8gc2VnbWVudCBhZ2Fpbi5cbiAgICAgICAgZm9yIChlbmQgPSBpbmRleDsgZW5kIDwgbGVuZ3RoOyBlbmQrKykge1xuICAgICAgICAgIGlmICh0aGlzLl9jaGFySXNNYXBwaW5nU2VwYXJhdG9yKGFTdHIsIGVuZCkpIHtcbiAgICAgICAgICAgIGJyZWFrO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgICBzdHIgPSBhU3RyLnNsaWNlKGluZGV4LCBlbmQpO1xuXG4gICAgICAgIHNlZ21lbnQgPSBjYWNoZWRTZWdtZW50c1tzdHJdO1xuICAgICAgICBpZiAoc2VnbWVudCkge1xuICAgICAgICAgIGluZGV4ICs9IHN0ci5sZW5ndGg7XG4gICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgc2VnbWVudCA9IFtdO1xuICAgICAgICAgIHdoaWxlIChpbmRleCA8IGVuZCkge1xuICAgICAgICAgICAgYmFzZTY0VkxRLmRlY29kZShhU3RyLCBpbmRleCwgdGVtcCk7XG4gICAgICAgICAgICB2YWx1ZSA9IHRlbXAudmFsdWU7XG4gICAgICAgICAgICBpbmRleCA9IHRlbXAucmVzdDtcbiAgICAgICAgICAgIHNlZ21lbnQucHVzaCh2YWx1ZSk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgaWYgKHNlZ21lbnQubGVuZ3RoID09PSAyKSB7XG4gICAgICAgICAgICB0aHJvdyBuZXcgRXJyb3IoJ0ZvdW5kIGEgc291cmNlLCBidXQgbm8gbGluZSBhbmQgY29sdW1uJyk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgaWYgKHNlZ21lbnQubGVuZ3RoID09PSAzKSB7XG4gICAgICAgICAgICB0aHJvdyBuZXcgRXJyb3IoJ0ZvdW5kIGEgc291cmNlIGFuZCBsaW5lLCBidXQgbm8gY29sdW1uJyk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgY2FjaGVkU2VnbWVudHNbc3RyXSA9IHNlZ21lbnQ7XG4gICAgICAgIH1cblxuICAgICAgICAvLyBHZW5lcmF0ZWQgY29sdW1uLlxuICAgICAgICBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiA9IHByZXZpb3VzR2VuZXJhdGVkQ29sdW1uICsgc2VnbWVudFswXTtcbiAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPiAxKSB7XG4gICAgICAgICAgLy8gT3JpZ2luYWwgc291cmNlLlxuICAgICAgICAgIG1hcHBpbmcuc291cmNlID0gcHJldmlvdXNTb3VyY2UgKyBzZWdtZW50WzFdO1xuICAgICAgICAgIHByZXZpb3VzU291cmNlICs9IHNlZ21lbnRbMV07XG5cbiAgICAgICAgICAvLyBPcmlnaW5hbCBsaW5lLlxuICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxMaW5lID0gcHJldmlvdXNPcmlnaW5hbExpbmUgKyBzZWdtZW50WzJdO1xuICAgICAgICAgIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gbWFwcGluZy5vcmlnaW5hbExpbmU7XG4gICAgICAgICAgLy8gTGluZXMgYXJlIHN0b3JlZCAwLWJhc2VkXG4gICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgKz0gMTtcblxuICAgICAgICAgIC8vIE9yaWdpbmFsIGNvbHVtbi5cbiAgICAgICAgICBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uID0gcHJldmlvdXNPcmlnaW5hbENvbHVtbiArIHNlZ21lbnRbM107XG4gICAgICAgICAgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IG1hcHBpbmcub3JpZ2luYWxDb2x1bW47XG5cbiAgICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPiA0KSB7XG4gICAgICAgICAgICAvLyBPcmlnaW5hbCBuYW1lLlxuICAgICAgICAgICAgbWFwcGluZy5uYW1lID0gcHJldmlvdXNOYW1lICsgc2VnbWVudFs0XTtcbiAgICAgICAgICAgIHByZXZpb3VzTmFtZSArPSBzZWdtZW50WzRdO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuXG4gICAgICAgIGdlbmVyYXRlZE1hcHBpbmdzLnB1c2gobWFwcGluZyk7XG4gICAgICAgIGlmICh0eXBlb2YgbWFwcGluZy5vcmlnaW5hbExpbmUgPT09ICdudW1iZXInKSB7XG4gICAgICAgICAgb3JpZ2luYWxNYXBwaW5ncy5wdXNoKG1hcHBpbmcpO1xuICAgICAgICB9XG4gICAgICB9XG4gICAgfVxuXG4gICAgcXVpY2tTb3J0KGdlbmVyYXRlZE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkKTtcbiAgICB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBnZW5lcmF0ZWRNYXBwaW5ncztcblxuICAgIHF1aWNrU29ydChvcmlnaW5hbE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zKTtcbiAgICB0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncyA9IG9yaWdpbmFsTWFwcGluZ3M7XG4gIH07XG5cbi8qKlxuICogRmluZCB0aGUgbWFwcGluZyB0aGF0IGJlc3QgbWF0Y2hlcyB0aGUgaHlwb3RoZXRpY2FsIFwibmVlZGxlXCIgbWFwcGluZyB0aGF0XG4gKiB3ZSBhcmUgc2VhcmNoaW5nIGZvciBpbiB0aGUgZ2l2ZW4gXCJoYXlzdGFja1wiIG9mIG1hcHBpbmdzLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fZmluZE1hcHBpbmcgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9maW5kTWFwcGluZyhhTmVlZGxlLCBhTWFwcGluZ3MsIGFMaW5lTmFtZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgYUNvbHVtbk5hbWUsIGFDb21wYXJhdG9yLCBhQmlhcykge1xuICAgIC8vIFRvIHJldHVybiB0aGUgcG9zaXRpb24gd2UgYXJlIHNlYXJjaGluZyBmb3IsIHdlIG11c3QgZmlyc3QgZmluZCB0aGVcbiAgICAvLyBtYXBwaW5nIGZvciB0aGUgZ2l2ZW4gcG9zaXRpb24gYW5kIHRoZW4gcmV0dXJuIHRoZSBvcHBvc2l0ZSBwb3NpdGlvbiBpdFxuICAgIC8vIHBvaW50cyB0by4gQmVjYXVzZSB0aGUgbWFwcGluZ3MgYXJlIHNvcnRlZCwgd2UgY2FuIHVzZSBiaW5hcnkgc2VhcmNoIHRvXG4gICAgLy8gZmluZCB0aGUgYmVzdCBtYXBwaW5nLlxuXG4gICAgaWYgKGFOZWVkbGVbYUxpbmVOYW1lXSA8PSAwKSB7XG4gICAgICB0aHJvdyBuZXcgVHlwZUVycm9yKCdMaW5lIG11c3QgYmUgZ3JlYXRlciB0aGFuIG9yIGVxdWFsIHRvIDEsIGdvdCAnXG4gICAgICAgICAgICAgICAgICAgICAgICAgICsgYU5lZWRsZVthTGluZU5hbWVdKTtcbiAgICB9XG4gICAgaWYgKGFOZWVkbGVbYUNvbHVtbk5hbWVdIDwgMCkge1xuICAgICAgdGhyb3cgbmV3IFR5cGVFcnJvcignQ29sdW1uIG11c3QgYmUgZ3JlYXRlciB0aGFuIG9yIGVxdWFsIHRvIDAsIGdvdCAnXG4gICAgICAgICAgICAgICAgICAgICAgICAgICsgYU5lZWRsZVthQ29sdW1uTmFtZV0pO1xuICAgIH1cblxuICAgIHJldHVybiBiaW5hcnlTZWFyY2guc2VhcmNoKGFOZWVkbGUsIGFNYXBwaW5ncywgYUNvbXBhcmF0b3IsIGFCaWFzKTtcbiAgfTtcblxuLyoqXG4gKiBDb21wdXRlIHRoZSBsYXN0IGNvbHVtbiBmb3IgZWFjaCBnZW5lcmF0ZWQgbWFwcGluZy4gVGhlIGxhc3QgY29sdW1uIGlzXG4gKiBpbmNsdXNpdmUuXG4gKi9cbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmNvbXB1dGVDb2x1bW5TcGFucyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2NvbXB1dGVDb2x1bW5TcGFucygpIHtcbiAgICBmb3IgKHZhciBpbmRleCA9IDA7IGluZGV4IDwgdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3MubGVuZ3RoOyArK2luZGV4KSB7XG4gICAgICB2YXIgbWFwcGluZyA9IHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgLy8gTWFwcGluZ3MgZG8gbm90IGNvbnRhaW4gYSBmaWVsZCBmb3IgdGhlIGxhc3QgZ2VuZXJhdGVkIGNvbHVtbnQuIFdlXG4gICAgICAvLyBjYW4gY29tZSB1cCB3aXRoIGFuIG9wdGltaXN0aWMgZXN0aW1hdGUsIGhvd2V2ZXIsIGJ5IGFzc3VtaW5nIHRoYXRcbiAgICAgIC8vIG1hcHBpbmdzIGFyZSBjb250aWd1b3VzIChpLmUuIGdpdmVuIHR3byBjb25zZWN1dGl2ZSBtYXBwaW5ncywgdGhlXG4gICAgICAvLyBmaXJzdCBtYXBwaW5nIGVuZHMgd2hlcmUgdGhlIHNlY29uZCBvbmUgc3RhcnRzKS5cbiAgICAgIGlmIChpbmRleCArIDEgPCB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5ncy5sZW5ndGgpIHtcbiAgICAgICAgdmFyIG5leHRNYXBwaW5nID0gdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3NbaW5kZXggKyAxXTtcblxuICAgICAgICBpZiAobWFwcGluZy5nZW5lcmF0ZWRMaW5lID09PSBuZXh0TWFwcGluZy5nZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgICAgbWFwcGluZy5sYXN0R2VuZXJhdGVkQ29sdW1uID0gbmV4dE1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uIC0gMTtcbiAgICAgICAgICBjb250aW51ZTtcbiAgICAgICAgfVxuICAgICAgfVxuXG4gICAgICAvLyBUaGUgbGFzdCBtYXBwaW5nIGZvciBlYWNoIGxpbmUgc3BhbnMgdGhlIGVudGlyZSBsaW5lLlxuICAgICAgbWFwcGluZy5sYXN0R2VuZXJhdGVkQ29sdW1uID0gSW5maW5pdHk7XG4gICAgfVxuICB9O1xuXG4vKipcbiAqIFJldHVybnMgdGhlIG9yaWdpbmFsIHNvdXJjZSwgbGluZSwgYW5kIGNvbHVtbiBpbmZvcm1hdGlvbiBmb3IgdGhlIGdlbmVyYXRlZFxuICogc291cmNlJ3MgbGluZSBhbmQgY29sdW1uIHBvc2l0aW9ucyBwcm92aWRlZC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgYW4gb2JqZWN0XG4gKiB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLiAgVGhlIGxpbmUgbnVtYmVyXG4gKiAgICAgaXMgMS1iYXNlZC5cbiAqICAgLSBjb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLiAgVGhlIGNvbHVtblxuICogICAgIG51bWJlciBpcyAwLWJhc2VkLlxuICogICAtIGJpYXM6IEVpdGhlciAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gKiAgICAgJ1NvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAqICAgICBzZWFyY2hpbmcgZm9yLCByZXNwZWN0aXZlbHksIGlmIHRoZSBleGFjdCBlbGVtZW50IGNhbm5vdCBiZSBmb3VuZC5cbiAqICAgICBEZWZhdWx0cyB0byAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnLlxuICpcbiAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIHNvdXJjZTogVGhlIG9yaWdpbmFsIHNvdXJjZSBmaWxlLCBvciBudWxsLlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLiAgVGhlXG4gKiAgICAgbGluZSBudW1iZXIgaXMgMS1iYXNlZC5cbiAqICAgLSBjb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UsIG9yIG51bGwuICBUaGVcbiAqICAgICBjb2x1bW4gbnVtYmVyIGlzIDAtYmFzZWQuXG4gKiAgIC0gbmFtZTogVGhlIG9yaWdpbmFsIGlkZW50aWZpZXIsIG9yIG51bGwuXG4gKi9cbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLm9yaWdpbmFsUG9zaXRpb25Gb3IgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9vcmlnaW5hbFBvc2l0aW9uRm9yKGFBcmdzKSB7XG4gICAgdmFyIG5lZWRsZSA9IHtcbiAgICAgIGdlbmVyYXRlZExpbmU6IHV0aWwuZ2V0QXJnKGFBcmdzLCAnbGluZScpLFxuICAgICAgZ2VuZXJhdGVkQ29sdW1uOiB1dGlsLmdldEFyZyhhQXJncywgJ2NvbHVtbicpXG4gICAgfTtcblxuICAgIHZhciBpbmRleCA9IHRoaXMuX2ZpbmRNYXBwaW5nKFxuICAgICAgbmVlZGxlLFxuICAgICAgdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3MsXG4gICAgICBcImdlbmVyYXRlZExpbmVcIixcbiAgICAgIFwiZ2VuZXJhdGVkQ29sdW1uXCIsXG4gICAgICB1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkLFxuICAgICAgdXRpbC5nZXRBcmcoYUFyZ3MsICdiaWFzJywgU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQpXG4gICAgKTtcblxuICAgIGlmIChpbmRleCA+PSAwKSB7XG4gICAgICB2YXIgbWFwcGluZyA9IHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgaWYgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA9PT0gbmVlZGxlLmdlbmVyYXRlZExpbmUpIHtcbiAgICAgICAgdmFyIHNvdXJjZSA9IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdzb3VyY2UnLCBudWxsKTtcbiAgICAgICAgaWYgKHNvdXJjZSAhPT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZSA9IHRoaXMuX3NvdXJjZXMuYXQoc291cmNlKTtcbiAgICAgICAgICBzb3VyY2UgPSB1dGlsLmNvbXB1dGVTb3VyY2VVUkwodGhpcy5zb3VyY2VSb290LCBzb3VyY2UsIHRoaXMuX3NvdXJjZU1hcFVSTCk7XG4gICAgICAgIH1cbiAgICAgICAgdmFyIG5hbWUgPSB1dGlsLmdldEFyZyhtYXBwaW5nLCAnbmFtZScsIG51bGwpO1xuICAgICAgICBpZiAobmFtZSAhPT0gbnVsbCkge1xuICAgICAgICAgIG5hbWUgPSB0aGlzLl9uYW1lcy5hdChuYW1lKTtcbiAgICAgICAgfVxuICAgICAgICByZXR1cm4ge1xuICAgICAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgICAgIGxpbmU6IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdvcmlnaW5hbExpbmUnLCBudWxsKSxcbiAgICAgICAgICBjb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdvcmlnaW5hbENvbHVtbicsIG51bGwpLFxuICAgICAgICAgIG5hbWU6IG5hbWVcbiAgICAgICAgfTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICByZXR1cm4ge1xuICAgICAgc291cmNlOiBudWxsLFxuICAgICAgbGluZTogbnVsbCxcbiAgICAgIGNvbHVtbjogbnVsbCxcbiAgICAgIG5hbWU6IG51bGxcbiAgICB9O1xuICB9O1xuXG4vKipcbiAqIFJldHVybiB0cnVlIGlmIHdlIGhhdmUgdGhlIHNvdXJjZSBjb250ZW50IGZvciBldmVyeSBzb3VyY2UgaW4gdGhlIHNvdXJjZVxuICogbWFwLCBmYWxzZSBvdGhlcndpc2UuXG4gKi9cbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzID1cbiAgZnVuY3Rpb24gQmFzaWNTb3VyY2VNYXBDb25zdW1lcl9oYXNDb250ZW50c09mQWxsU291cmNlcygpIHtcbiAgICBpZiAoIXRoaXMuc291cmNlc0NvbnRlbnQpIHtcbiAgICAgIHJldHVybiBmYWxzZTtcbiAgICB9XG4gICAgcmV0dXJuIHRoaXMuc291cmNlc0NvbnRlbnQubGVuZ3RoID49IHRoaXMuX3NvdXJjZXMuc2l6ZSgpICYmXG4gICAgICAhdGhpcy5zb3VyY2VzQ29udGVudC5zb21lKGZ1bmN0aW9uIChzYykgeyByZXR1cm4gc2MgPT0gbnVsbDsgfSk7XG4gIH07XG5cbi8qKlxuICogUmV0dXJucyB0aGUgb3JpZ2luYWwgc291cmNlIGNvbnRlbnQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIHRoZSB1cmwgb2YgdGhlXG4gKiBvcmlnaW5hbCBzb3VyY2UgZmlsZS4gUmV0dXJucyBudWxsIGlmIG5vIG9yaWdpbmFsIHNvdXJjZSBjb250ZW50IGlzXG4gKiBhdmFpbGFibGUuXG4gKi9cbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLnNvdXJjZUNvbnRlbnRGb3IgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9zb3VyY2VDb250ZW50Rm9yKGFTb3VyY2UsIG51bGxPbk1pc3NpbmcpIHtcbiAgICBpZiAoIXRoaXMuc291cmNlc0NvbnRlbnQpIHtcbiAgICAgIHJldHVybiBudWxsO1xuICAgIH1cblxuICAgIHZhciBpbmRleCA9IHRoaXMuX2ZpbmRTb3VyY2VJbmRleChhU291cmNlKTtcbiAgICBpZiAoaW5kZXggPj0gMCkge1xuICAgICAgcmV0dXJuIHRoaXMuc291cmNlc0NvbnRlbnRbaW5kZXhdO1xuICAgIH1cblxuICAgIHZhciByZWxhdGl2ZVNvdXJjZSA9IGFTb3VyY2U7XG4gICAgaWYgKHRoaXMuc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICByZWxhdGl2ZVNvdXJjZSA9IHV0aWwucmVsYXRpdmUodGhpcy5zb3VyY2VSb290LCByZWxhdGl2ZVNvdXJjZSk7XG4gICAgfVxuXG4gICAgdmFyIHVybDtcbiAgICBpZiAodGhpcy5zb3VyY2VSb290ICE9IG51bGxcbiAgICAgICAgJiYgKHVybCA9IHV0aWwudXJsUGFyc2UodGhpcy5zb3VyY2VSb290KSkpIHtcbiAgICAgIC8vIFhYWDogZmlsZTovLyBVUklzIGFuZCBhYnNvbHV0ZSBwYXRocyBsZWFkIHRvIHVuZXhwZWN0ZWQgYmVoYXZpb3IgZm9yXG4gICAgICAvLyBtYW55IHVzZXJzLiBXZSBjYW4gaGVscCB0aGVtIG91dCB3aGVuIHRoZXkgZXhwZWN0IGZpbGU6Ly8gVVJJcyB0b1xuICAgICAgLy8gYmVoYXZlIGxpa2UgaXQgd291bGQgaWYgdGhleSB3ZXJlIHJ1bm5pbmcgYSBsb2NhbCBIVFRQIHNlcnZlci4gU2VlXG4gICAgICAvLyBodHRwczovL2J1Z3ppbGxhLm1vemlsbGEub3JnL3Nob3dfYnVnLmNnaT9pZD04ODU1OTcuXG4gICAgICB2YXIgZmlsZVVyaUFic1BhdGggPSByZWxhdGl2ZVNvdXJjZS5yZXBsYWNlKC9eZmlsZTpcXC9cXC8vLCBcIlwiKTtcbiAgICAgIGlmICh1cmwuc2NoZW1lID09IFwiZmlsZVwiXG4gICAgICAgICAgJiYgdGhpcy5fc291cmNlcy5oYXMoZmlsZVVyaUFic1BhdGgpKSB7XG4gICAgICAgIHJldHVybiB0aGlzLnNvdXJjZXNDb250ZW50W3RoaXMuX3NvdXJjZXMuaW5kZXhPZihmaWxlVXJpQWJzUGF0aCldXG4gICAgICB9XG5cbiAgICAgIGlmICgoIXVybC5wYXRoIHx8IHVybC5wYXRoID09IFwiL1wiKVxuICAgICAgICAgICYmIHRoaXMuX3NvdXJjZXMuaGFzKFwiL1wiICsgcmVsYXRpdmVTb3VyY2UpKSB7XG4gICAgICAgIHJldHVybiB0aGlzLnNvdXJjZXNDb250ZW50W3RoaXMuX3NvdXJjZXMuaW5kZXhPZihcIi9cIiArIHJlbGF0aXZlU291cmNlKV07XG4gICAgICB9XG4gICAgfVxuXG4gICAgLy8gVGhpcyBmdW5jdGlvbiBpcyB1c2VkIHJlY3Vyc2l2ZWx5IGZyb21cbiAgICAvLyBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLnNvdXJjZUNvbnRlbnRGb3IuIEluIHRoYXQgY2FzZSwgd2VcbiAgICAvLyBkb24ndCB3YW50IHRvIHRocm93IGlmIHdlIGNhbid0IGZpbmQgdGhlIHNvdXJjZSAtIHdlIGp1c3Qgd2FudCB0b1xuICAgIC8vIHJldHVybiBudWxsLCBzbyB3ZSBwcm92aWRlIGEgZmxhZyB0byBleGl0IGdyYWNlZnVsbHkuXG4gICAgaWYgKG51bGxPbk1pc3NpbmcpIHtcbiAgICAgIHJldHVybiBudWxsO1xuICAgIH1cbiAgICBlbHNlIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcignXCInICsgcmVsYXRpdmVTb3VyY2UgKyAnXCIgaXMgbm90IGluIHRoZSBTb3VyY2VNYXAuJyk7XG4gICAgfVxuICB9O1xuXG4vKipcbiAqIFJldHVybnMgdGhlIGdlbmVyYXRlZCBsaW5lIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBvcmlnaW5hbCBzb3VyY2UsXG4gKiBsaW5lLCBhbmQgY29sdW1uIHBvc2l0aW9ucyBwcm92aWRlZC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgYW4gb2JqZWN0IHdpdGhcbiAqIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gc291cmNlOiBUaGUgZmlsZW5hbWUgb2YgdGhlIG9yaWdpbmFsIHNvdXJjZS5cbiAqICAgLSBsaW5lOiBUaGUgbGluZSBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZS4gIFRoZSBsaW5lIG51bWJlclxuICogICAgIGlzIDEtYmFzZWQuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLiAgVGhlIGNvbHVtblxuICogICAgIG51bWJlciBpcyAwLWJhc2VkLlxuICogICAtIGJpYXM6IEVpdGhlciAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gKiAgICAgJ1NvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAqICAgICBzZWFyY2hpbmcgZm9yLCByZXNwZWN0aXZlbHksIGlmIHRoZSBleGFjdCBlbGVtZW50IGNhbm5vdCBiZSBmb3VuZC5cbiAqICAgICBEZWZhdWx0cyB0byAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnLlxuICpcbiAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC4gIFRoZVxuICogICAgIGxpbmUgbnVtYmVyIGlzIDEtYmFzZWQuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC5cbiAqICAgICBUaGUgY29sdW1uIG51bWJlciBpcyAwLWJhc2VkLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5nZW5lcmF0ZWRQb3NpdGlvbkZvciA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2dlbmVyYXRlZFBvc2l0aW9uRm9yKGFBcmdzKSB7XG4gICAgdmFyIHNvdXJjZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnc291cmNlJyk7XG4gICAgc291cmNlID0gdGhpcy5fZmluZFNvdXJjZUluZGV4KHNvdXJjZSk7XG4gICAgaWYgKHNvdXJjZSA8IDApIHtcbiAgICAgIHJldHVybiB7XG4gICAgICAgIGxpbmU6IG51bGwsXG4gICAgICAgIGNvbHVtbjogbnVsbCxcbiAgICAgICAgbGFzdENvbHVtbjogbnVsbFxuICAgICAgfTtcbiAgICB9XG5cbiAgICB2YXIgbmVlZGxlID0ge1xuICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICBvcmlnaW5hbExpbmU6IHV0aWwuZ2V0QXJnKGFBcmdzLCAnbGluZScpLFxuICAgICAgb3JpZ2luYWxDb2x1bW46IHV0aWwuZ2V0QXJnKGFBcmdzLCAnY29sdW1uJylcbiAgICB9O1xuXG4gICAgdmFyIGluZGV4ID0gdGhpcy5fZmluZE1hcHBpbmcoXG4gICAgICBuZWVkbGUsXG4gICAgICB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzLFxuICAgICAgXCJvcmlnaW5hbExpbmVcIixcbiAgICAgIFwib3JpZ2luYWxDb2x1bW5cIixcbiAgICAgIHV0aWwuY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMsXG4gICAgICB1dGlsLmdldEFyZyhhQXJncywgJ2JpYXMnLCBTb3VyY2VNYXBDb25zdW1lci5HUkVBVEVTVF9MT1dFUl9CT1VORClcbiAgICApO1xuXG4gICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgIHZhciBtYXBwaW5nID0gdGhpcy5fb3JpZ2luYWxNYXBwaW5nc1tpbmRleF07XG5cbiAgICAgIGlmIChtYXBwaW5nLnNvdXJjZSA9PT0gbmVlZGxlLnNvdXJjZSkge1xuICAgICAgICByZXR1cm4ge1xuICAgICAgICAgIGxpbmU6IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRMaW5lJywgbnVsbCksXG4gICAgICAgICAgY29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkQ29sdW1uJywgbnVsbCksXG4gICAgICAgICAgbGFzdENvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ2xhc3RHZW5lcmF0ZWRDb2x1bW4nLCBudWxsKVxuICAgICAgICB9O1xuICAgICAgfVxuICAgIH1cblxuICAgIHJldHVybiB7XG4gICAgICBsaW5lOiBudWxsLFxuICAgICAgY29sdW1uOiBudWxsLFxuICAgICAgbGFzdENvbHVtbjogbnVsbFxuICAgIH07XG4gIH07XG5cbmV4cG9ydHMuQmFzaWNTb3VyY2VNYXBDb25zdW1lciA9IEJhc2ljU291cmNlTWFwQ29uc3VtZXI7XG5cbi8qKlxuICogQW4gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyIGluc3RhbmNlIHJlcHJlc2VudHMgYSBwYXJzZWQgc291cmNlIG1hcCB3aGljaFxuICogd2UgY2FuIHF1ZXJ5IGZvciBpbmZvcm1hdGlvbi4gSXQgZGlmZmVycyBmcm9tIEJhc2ljU291cmNlTWFwQ29uc3VtZXIgaW5cbiAqIHRoYXQgaXQgdGFrZXMgXCJpbmRleGVkXCIgc291cmNlIG1hcHMgKGkuZS4gb25lcyB3aXRoIGEgXCJzZWN0aW9uc1wiIGZpZWxkKSBhc1xuICogaW5wdXQuXG4gKlxuICogVGhlIGZpcnN0IHBhcmFtZXRlciBpcyBhIHJhdyBzb3VyY2UgbWFwIChlaXRoZXIgYXMgYSBKU09OIHN0cmluZywgb3IgYWxyZWFkeVxuICogcGFyc2VkIHRvIGFuIG9iamVjdCkuIEFjY29yZGluZyB0byB0aGUgc3BlYyBmb3IgaW5kZXhlZCBzb3VyY2UgbWFwcywgdGhleVxuICogaGF2ZSB0aGUgZm9sbG93aW5nIGF0dHJpYnV0ZXM6XG4gKlxuICogICAtIHZlcnNpb246IFdoaWNoIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXAgc3BlYyB0aGlzIG1hcCBpcyBmb2xsb3dpbmcuXG4gKiAgIC0gZmlsZTogT3B0aW9uYWwuIFRoZSBnZW5lcmF0ZWQgZmlsZSB0aGlzIHNvdXJjZSBtYXAgaXMgYXNzb2NpYXRlZCB3aXRoLlxuICogICAtIHNlY3Rpb25zOiBBIGxpc3Qgb2Ygc2VjdGlvbiBkZWZpbml0aW9ucy5cbiAqXG4gKiBFYWNoIHZhbHVlIHVuZGVyIHRoZSBcInNlY3Rpb25zXCIgZmllbGQgaGFzIHR3byBmaWVsZHM6XG4gKiAgIC0gb2Zmc2V0OiBUaGUgb2Zmc2V0IGludG8gdGhlIG9yaWdpbmFsIHNwZWNpZmllZCBhdCB3aGljaCB0aGlzIHNlY3Rpb25cbiAqICAgICAgIGJlZ2lucyB0byBhcHBseSwgZGVmaW5lZCBhcyBhbiBvYmplY3Qgd2l0aCBhIFwibGluZVwiIGFuZCBcImNvbHVtblwiXG4gKiAgICAgICBmaWVsZC5cbiAqICAgLSBtYXA6IEEgc291cmNlIG1hcCBkZWZpbml0aW9uLiBUaGlzIHNvdXJjZSBtYXAgY291bGQgYWxzbyBiZSBpbmRleGVkLFxuICogICAgICAgYnV0IGRvZXNuJ3QgaGF2ZSB0byBiZS5cbiAqXG4gKiBJbnN0ZWFkIG9mIHRoZSBcIm1hcFwiIGZpZWxkLCBpdCdzIGFsc28gcG9zc2libGUgdG8gaGF2ZSBhIFwidXJsXCIgZmllbGRcbiAqIHNwZWNpZnlpbmcgYSBVUkwgdG8gcmV0cmlldmUgYSBzb3VyY2UgbWFwIGZyb20sIGJ1dCB0aGF0J3MgY3VycmVudGx5XG4gKiB1bnN1cHBvcnRlZC5cbiAqXG4gKiBIZXJlJ3MgYW4gZXhhbXBsZSBzb3VyY2UgbWFwLCB0YWtlbiBmcm9tIHRoZSBzb3VyY2UgbWFwIHNwZWNbMF0sIGJ1dFxuICogbW9kaWZpZWQgdG8gb21pdCBhIHNlY3Rpb24gd2hpY2ggdXNlcyB0aGUgXCJ1cmxcIiBmaWVsZC5cbiAqXG4gKiAge1xuICogICAgdmVyc2lvbiA6IDMsXG4gKiAgICBmaWxlOiBcImFwcC5qc1wiLFxuICogICAgc2VjdGlvbnM6IFt7XG4gKiAgICAgIG9mZnNldDoge2xpbmU6MTAwLCBjb2x1bW46MTB9LFxuICogICAgICBtYXA6IHtcbiAqICAgICAgICB2ZXJzaW9uIDogMyxcbiAqICAgICAgICBmaWxlOiBcInNlY3Rpb24uanNcIixcbiAqICAgICAgICBzb3VyY2VzOiBbXCJmb28uanNcIiwgXCJiYXIuanNcIl0sXG4gKiAgICAgICAgbmFtZXM6IFtcInNyY1wiLCBcIm1hcHNcIiwgXCJhcmVcIiwgXCJmdW5cIl0sXG4gKiAgICAgICAgbWFwcGluZ3M6IFwiQUFBQSxFOztBQkNERTtcIlxuICogICAgICB9XG4gKiAgICB9XSxcbiAqICB9XG4gKlxuICogVGhlIHNlY29uZCBwYXJhbWV0ZXIsIGlmIGdpdmVuLCBpcyBhIHN0cmluZyB3aG9zZSB2YWx1ZSBpcyB0aGUgVVJMXG4gKiBhdCB3aGljaCB0aGUgc291cmNlIG1hcCB3YXMgZm91bmQuICBUaGlzIFVSTCBpcyB1c2VkIHRvIGNvbXB1dGUgdGhlXG4gKiBzb3VyY2VzIGFycmF5LlxuICpcbiAqIFswXTogaHR0cHM6Ly9kb2NzLmdvb2dsZS5jb20vZG9jdW1lbnQvZC8xVTFSR0FlaFF3UnlwVVRvdkYxS1JscGlPRnplMGItXzJnYzZmQUgwS1kway9lZGl0I2hlYWRpbmc9aC41MzVlczN4ZXByZ3RcbiAqL1xuZnVuY3Rpb24gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyKGFTb3VyY2VNYXAsIGFTb3VyY2VNYXBVUkwpIHtcbiAgdmFyIHNvdXJjZU1hcCA9IGFTb3VyY2VNYXA7XG4gIGlmICh0eXBlb2YgYVNvdXJjZU1hcCA9PT0gJ3N0cmluZycpIHtcbiAgICBzb3VyY2VNYXAgPSB1dGlsLnBhcnNlU291cmNlTWFwSW5wdXQoYVNvdXJjZU1hcCk7XG4gIH1cblxuICB2YXIgdmVyc2lvbiA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ3ZlcnNpb24nKTtcbiAgdmFyIHNlY3Rpb25zID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnc2VjdGlvbnMnKTtcblxuICBpZiAodmVyc2lvbiAhPSB0aGlzLl92ZXJzaW9uKSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKCdVbnN1cHBvcnRlZCB2ZXJzaW9uOiAnICsgdmVyc2lvbik7XG4gIH1cblxuICB0aGlzLl9zb3VyY2VzID0gbmV3IEFycmF5U2V0KCk7XG4gIHRoaXMuX25hbWVzID0gbmV3IEFycmF5U2V0KCk7XG5cbiAgdmFyIGxhc3RPZmZzZXQgPSB7XG4gICAgbGluZTogLTEsXG4gICAgY29sdW1uOiAwXG4gIH07XG4gIHRoaXMuX3NlY3Rpb25zID0gc2VjdGlvbnMubWFwKGZ1bmN0aW9uIChzKSB7XG4gICAgaWYgKHMudXJsKSB7XG4gICAgICAvLyBUaGUgdXJsIGZpZWxkIHdpbGwgcmVxdWlyZSBzdXBwb3J0IGZvciBhc3luY2hyb25pY2l0eS5cbiAgICAgIC8vIFNlZSBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL2lzc3Vlcy8xNlxuICAgICAgdGhyb3cgbmV3IEVycm9yKCdTdXBwb3J0IGZvciB1cmwgZmllbGQgaW4gc2VjdGlvbnMgbm90IGltcGxlbWVudGVkLicpO1xuICAgIH1cbiAgICB2YXIgb2Zmc2V0ID0gdXRpbC5nZXRBcmcocywgJ29mZnNldCcpO1xuICAgIHZhciBvZmZzZXRMaW5lID0gdXRpbC5nZXRBcmcob2Zmc2V0LCAnbGluZScpO1xuICAgIHZhciBvZmZzZXRDb2x1bW4gPSB1dGlsLmdldEFyZyhvZmZzZXQsICdjb2x1bW4nKTtcblxuICAgIGlmIChvZmZzZXRMaW5lIDwgbGFzdE9mZnNldC5saW5lIHx8XG4gICAgICAgIChvZmZzZXRMaW5lID09PSBsYXN0T2Zmc2V0LmxpbmUgJiYgb2Zmc2V0Q29sdW1uIDwgbGFzdE9mZnNldC5jb2x1bW4pKSB7XG4gICAgICB0aHJvdyBuZXcgRXJyb3IoJ1NlY3Rpb24gb2Zmc2V0cyBtdXN0IGJlIG9yZGVyZWQgYW5kIG5vbi1vdmVybGFwcGluZy4nKTtcbiAgICB9XG4gICAgbGFzdE9mZnNldCA9IG9mZnNldDtcblxuICAgIHJldHVybiB7XG4gICAgICBnZW5lcmF0ZWRPZmZzZXQ6IHtcbiAgICAgICAgLy8gVGhlIG9mZnNldCBmaWVsZHMgYXJlIDAtYmFzZWQsIGJ1dCB3ZSB1c2UgMS1iYXNlZCBpbmRpY2VzIHdoZW5cbiAgICAgICAgLy8gZW5jb2RpbmcvZGVjb2RpbmcgZnJvbSBWTFEuXG4gICAgICAgIGdlbmVyYXRlZExpbmU6IG9mZnNldExpbmUgKyAxLFxuICAgICAgICBnZW5lcmF0ZWRDb2x1bW46IG9mZnNldENvbHVtbiArIDFcbiAgICAgIH0sXG4gICAgICBjb25zdW1lcjogbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuZ2V0QXJnKHMsICdtYXAnKSwgYVNvdXJjZU1hcFVSTClcbiAgICB9XG4gIH0pO1xufVxuXG5JbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlID0gT2JqZWN0LmNyZWF0ZShTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUpO1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5jb25zdHJ1Y3RvciA9IFNvdXJjZU1hcENvbnN1bWVyO1xuXG4vKipcbiAqIFRoZSB2ZXJzaW9uIG9mIHRoZSBzb3VyY2UgbWFwcGluZyBzcGVjIHRoYXQgd2UgYXJlIGNvbnN1bWluZy5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbi8qKlxuICogVGhlIGxpc3Qgb2Ygb3JpZ2luYWwgc291cmNlcy5cbiAqL1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUsICdzb3VyY2VzJywge1xuICBnZXQ6IGZ1bmN0aW9uICgpIHtcbiAgICB2YXIgc291cmNlcyA9IFtdO1xuICAgIGZvciAodmFyIGkgPSAwOyBpIDwgdGhpcy5fc2VjdGlvbnMubGVuZ3RoOyBpKyspIHtcbiAgICAgIGZvciAodmFyIGogPSAwOyBqIDwgdGhpcy5fc2VjdGlvbnNbaV0uY29uc3VtZXIuc291cmNlcy5sZW5ndGg7IGorKykge1xuICAgICAgICBzb3VyY2VzLnB1c2godGhpcy5fc2VjdGlvbnNbaV0uY29uc3VtZXIuc291cmNlc1tqXSk7XG4gICAgICB9XG4gICAgfVxuICAgIHJldHVybiBzb3VyY2VzO1xuICB9XG59KTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBvcmlnaW5hbCBzb3VyY2UsIGxpbmUsIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBnZW5lcmF0ZWRcbiAqIHNvdXJjZSdzIGxpbmUgYW5kIGNvbHVtbiBwb3NpdGlvbnMgcHJvdmlkZWQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIGFuIG9iamVjdFxuICogd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZS4gIFRoZSBsaW5lIG51bWJlclxuICogICAgIGlzIDEtYmFzZWQuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZS4gIFRoZSBjb2x1bW5cbiAqICAgICBudW1iZXIgaXMgMC1iYXNlZC5cbiAqXG4gKiBhbmQgYW4gb2JqZWN0IGlzIHJldHVybmVkIHdpdGggdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICpcbiAqICAgLSBzb3VyY2U6IFRoZSBvcmlnaW5hbCBzb3VyY2UgZmlsZSwgb3IgbnVsbC5cbiAqICAgLSBsaW5lOiBUaGUgbGluZSBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZSwgb3IgbnVsbC4gIFRoZVxuICogICAgIGxpbmUgbnVtYmVyIGlzIDEtYmFzZWQuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLiAgVGhlXG4gKiAgICAgY29sdW1uIG51bWJlciBpcyAwLWJhc2VkLlxuICogICAtIG5hbWU6IFRoZSBvcmlnaW5hbCBpZGVudGlmaWVyLCBvciBudWxsLlxuICovXG5JbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLm9yaWdpbmFsUG9zaXRpb25Gb3IgPVxuICBmdW5jdGlvbiBJbmRleGVkU291cmNlTWFwQ29uc3VtZXJfb3JpZ2luYWxQb3NpdGlvbkZvcihhQXJncykge1xuICAgIHZhciBuZWVkbGUgPSB7XG4gICAgICBnZW5lcmF0ZWRMaW5lOiB1dGlsLmdldEFyZyhhQXJncywgJ2xpbmUnKSxcbiAgICAgIGdlbmVyYXRlZENvbHVtbjogdXRpbC5nZXRBcmcoYUFyZ3MsICdjb2x1bW4nKVxuICAgIH07XG5cbiAgICAvLyBGaW5kIHRoZSBzZWN0aW9uIGNvbnRhaW5pbmcgdGhlIGdlbmVyYXRlZCBwb3NpdGlvbiB3ZSdyZSB0cnlpbmcgdG8gbWFwXG4gICAgLy8gdG8gYW4gb3JpZ2luYWwgcG9zaXRpb24uXG4gICAgdmFyIHNlY3Rpb25JbmRleCA9IGJpbmFyeVNlYXJjaC5zZWFyY2gobmVlZGxlLCB0aGlzLl9zZWN0aW9ucyxcbiAgICAgIGZ1bmN0aW9uKG5lZWRsZSwgc2VjdGlvbikge1xuICAgICAgICB2YXIgY21wID0gbmVlZGxlLmdlbmVyYXRlZExpbmUgLSBzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRMaW5lO1xuICAgICAgICBpZiAoY21wKSB7XG4gICAgICAgICAgcmV0dXJuIGNtcDtcbiAgICAgICAgfVxuXG4gICAgICAgIHJldHVybiAobmVlZGxlLmdlbmVyYXRlZENvbHVtbiAtXG4gICAgICAgICAgICAgICAgc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkQ29sdW1uKTtcbiAgICAgIH0pO1xuICAgIHZhciBzZWN0aW9uID0gdGhpcy5fc2VjdGlvbnNbc2VjdGlvbkluZGV4XTtcblxuICAgIGlmICghc2VjdGlvbikge1xuICAgICAgcmV0dXJuIHtcbiAgICAgICAgc291cmNlOiBudWxsLFxuICAgICAgICBsaW5lOiBudWxsLFxuICAgICAgICBjb2x1bW46IG51bGwsXG4gICAgICAgIG5hbWU6IG51bGxcbiAgICAgIH07XG4gICAgfVxuXG4gICAgcmV0dXJuIHNlY3Rpb24uY29uc3VtZXIub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiBuZWVkbGUuZ2VuZXJhdGVkTGluZSAtXG4gICAgICAgIChzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRMaW5lIC0gMSksXG4gICAgICBjb2x1bW46IG5lZWRsZS5nZW5lcmF0ZWRDb2x1bW4gLVxuICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSA9PT0gbmVlZGxlLmdlbmVyYXRlZExpbmVcbiAgICAgICAgID8gc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkQ29sdW1uIC0gMVxuICAgICAgICAgOiAwKSxcbiAgICAgIGJpYXM6IGFBcmdzLmJpYXNcbiAgICB9KTtcbiAgfTtcblxuLyoqXG4gKiBSZXR1cm4gdHJ1ZSBpZiB3ZSBoYXZlIHRoZSBzb3VyY2UgY29udGVudCBmb3IgZXZlcnkgc291cmNlIGluIHRoZSBzb3VyY2VcbiAqIG1hcCwgZmFsc2Ugb3RoZXJ3aXNlLlxuICovXG5JbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzID1cbiAgZnVuY3Rpb24gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyX2hhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzKCkge1xuICAgIHJldHVybiB0aGlzLl9zZWN0aW9ucy5ldmVyeShmdW5jdGlvbiAocykge1xuICAgICAgcmV0dXJuIHMuY29uc3VtZXIuaGFzQ29udGVudHNPZkFsbFNvdXJjZXMoKTtcbiAgICB9KTtcbiAgfTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBvcmlnaW5hbCBzb3VyY2UgY29udGVudC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgdGhlIHVybCBvZiB0aGVcbiAqIG9yaWdpbmFsIHNvdXJjZSBmaWxlLiBSZXR1cm5zIG51bGwgaWYgbm8gb3JpZ2luYWwgc291cmNlIGNvbnRlbnQgaXNcbiAqIGF2YWlsYWJsZS5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5zb3VyY2VDb250ZW50Rm9yID1cbiAgZnVuY3Rpb24gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyX3NvdXJjZUNvbnRlbnRGb3IoYVNvdXJjZSwgbnVsbE9uTWlzc2luZykge1xuICAgIGZvciAodmFyIGkgPSAwOyBpIDwgdGhpcy5fc2VjdGlvbnMubGVuZ3RoOyBpKyspIHtcbiAgICAgIHZhciBzZWN0aW9uID0gdGhpcy5fc2VjdGlvbnNbaV07XG5cbiAgICAgIHZhciBjb250ZW50ID0gc2VjdGlvbi5jb25zdW1lci5zb3VyY2VDb250ZW50Rm9yKGFTb3VyY2UsIHRydWUpO1xuICAgICAgaWYgKGNvbnRlbnQpIHtcbiAgICAgICAgcmV0dXJuIGNvbnRlbnQ7XG4gICAgICB9XG4gICAgfVxuICAgIGlmIChudWxsT25NaXNzaW5nKSB7XG4gICAgICByZXR1cm4gbnVsbDtcbiAgICB9XG4gICAgZWxzZSB7XG4gICAgICB0aHJvdyBuZXcgRXJyb3IoJ1wiJyArIGFTb3VyY2UgKyAnXCIgaXMgbm90IGluIHRoZSBTb3VyY2VNYXAuJyk7XG4gICAgfVxuICB9O1xuXG4vKipcbiAqIFJldHVybnMgdGhlIGdlbmVyYXRlZCBsaW5lIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBvcmlnaW5hbCBzb3VyY2UsXG4gKiBsaW5lLCBhbmQgY29sdW1uIHBvc2l0aW9ucyBwcm92aWRlZC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgYW4gb2JqZWN0IHdpdGhcbiAqIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gc291cmNlOiBUaGUgZmlsZW5hbWUgb2YgdGhlIG9yaWdpbmFsIHNvdXJjZS5cbiAqICAgLSBsaW5lOiBUaGUgbGluZSBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZS4gIFRoZSBsaW5lIG51bWJlclxuICogICAgIGlzIDEtYmFzZWQuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLiAgVGhlIGNvbHVtblxuICogICAgIG51bWJlciBpcyAwLWJhc2VkLlxuICpcbiAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC4gIFRoZVxuICogICAgIGxpbmUgbnVtYmVyIGlzIDEtYmFzZWQuIFxuICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UsIG9yIG51bGwuXG4gKiAgICAgVGhlIGNvbHVtbiBudW1iZXIgaXMgMC1iYXNlZC5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5nZW5lcmF0ZWRQb3NpdGlvbkZvciA9XG4gIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9nZW5lcmF0ZWRQb3NpdGlvbkZvcihhQXJncykge1xuICAgIGZvciAodmFyIGkgPSAwOyBpIDwgdGhpcy5fc2VjdGlvbnMubGVuZ3RoOyBpKyspIHtcbiAgICAgIHZhciBzZWN0aW9uID0gdGhpcy5fc2VjdGlvbnNbaV07XG5cbiAgICAgIC8vIE9ubHkgY29uc2lkZXIgdGhpcyBzZWN0aW9uIGlmIHRoZSByZXF1ZXN0ZWQgc291cmNlIGlzIGluIHRoZSBsaXN0IG9mXG4gICAgICAvLyBzb3VyY2VzIG9mIHRoZSBjb25zdW1lci5cbiAgICAgIGlmIChzZWN0aW9uLmNvbnN1bWVyLl9maW5kU291cmNlSW5kZXgodXRpbC5nZXRBcmcoYUFyZ3MsICdzb3VyY2UnKSkgPT09IC0xKSB7XG4gICAgICAgIGNvbnRpbnVlO1xuICAgICAgfVxuICAgICAgdmFyIGdlbmVyYXRlZFBvc2l0aW9uID0gc2VjdGlvbi5jb25zdW1lci5nZW5lcmF0ZWRQb3NpdGlvbkZvcihhQXJncyk7XG4gICAgICBpZiAoZ2VuZXJhdGVkUG9zaXRpb24pIHtcbiAgICAgICAgdmFyIHJldCA9IHtcbiAgICAgICAgICBsaW5lOiBnZW5lcmF0ZWRQb3NpdGlvbi5saW5lICtcbiAgICAgICAgICAgIChzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRMaW5lIC0gMSksXG4gICAgICAgICAgY29sdW1uOiBnZW5lcmF0ZWRQb3NpdGlvbi5jb2x1bW4gK1xuICAgICAgICAgICAgKHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmUgPT09IGdlbmVyYXRlZFBvc2l0aW9uLmxpbmVcbiAgICAgICAgICAgICA/IHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZENvbHVtbiAtIDFcbiAgICAgICAgICAgICA6IDApXG4gICAgICAgIH07XG4gICAgICAgIHJldHVybiByZXQ7XG4gICAgICB9XG4gICAgfVxuXG4gICAgcmV0dXJuIHtcbiAgICAgIGxpbmU6IG51bGwsXG4gICAgICBjb2x1bW46IG51bGxcbiAgICB9O1xuICB9O1xuXG4vKipcbiAqIFBhcnNlIHRoZSBtYXBwaW5ncyBpbiBhIHN0cmluZyBpbiB0byBhIGRhdGEgc3RydWN0dXJlIHdoaWNoIHdlIGNhbiBlYXNpbHlcbiAqIHF1ZXJ5ICh0aGUgb3JkZXJlZCBhcnJheXMgaW4gdGhlIGB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3NgIGFuZFxuICogYHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzYCBwcm9wZXJ0aWVzKS5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fcGFyc2VNYXBwaW5ncyA9XG4gIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9wYXJzZU1hcHBpbmdzKGFTdHIsIGFTb3VyY2VSb290KSB7XG4gICAgdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzID0gW107XG4gICAgdGhpcy5fX29yaWdpbmFsTWFwcGluZ3MgPSBbXTtcbiAgICBmb3IgKHZhciBpID0gMDsgaSA8IHRoaXMuX3NlY3Rpb25zLmxlbmd0aDsgaSsrKSB7XG4gICAgICB2YXIgc2VjdGlvbiA9IHRoaXMuX3NlY3Rpb25zW2ldO1xuICAgICAgdmFyIHNlY3Rpb25NYXBwaW5ncyA9IHNlY3Rpb24uY29uc3VtZXIuX2dlbmVyYXRlZE1hcHBpbmdzO1xuICAgICAgZm9yICh2YXIgaiA9IDA7IGogPCBzZWN0aW9uTWFwcGluZ3MubGVuZ3RoOyBqKyspIHtcbiAgICAgICAgdmFyIG1hcHBpbmcgPSBzZWN0aW9uTWFwcGluZ3Nbal07XG5cbiAgICAgICAgdmFyIHNvdXJjZSA9IHNlY3Rpb24uY29uc3VtZXIuX3NvdXJjZXMuYXQobWFwcGluZy5zb3VyY2UpO1xuICAgICAgICBzb3VyY2UgPSB1dGlsLmNvbXB1dGVTb3VyY2VVUkwoc2VjdGlvbi5jb25zdW1lci5zb3VyY2VSb290LCBzb3VyY2UsIHRoaXMuX3NvdXJjZU1hcFVSTCk7XG4gICAgICAgIHRoaXMuX3NvdXJjZXMuYWRkKHNvdXJjZSk7XG4gICAgICAgIHNvdXJjZSA9IHRoaXMuX3NvdXJjZXMuaW5kZXhPZihzb3VyY2UpO1xuXG4gICAgICAgIHZhciBuYW1lID0gbnVsbDtcbiAgICAgICAgaWYgKG1hcHBpbmcubmFtZSkge1xuICAgICAgICAgIG5hbWUgPSBzZWN0aW9uLmNvbnN1bWVyLl9uYW1lcy5hdChtYXBwaW5nLm5hbWUpO1xuICAgICAgICAgIHRoaXMuX25hbWVzLmFkZChuYW1lKTtcbiAgICAgICAgICBuYW1lID0gdGhpcy5fbmFtZXMuaW5kZXhPZihuYW1lKTtcbiAgICAgICAgfVxuXG4gICAgICAgIC8vIFRoZSBtYXBwaW5ncyBjb21pbmcgZnJvbSB0aGUgY29uc3VtZXIgZm9yIHRoZSBzZWN0aW9uIGhhdmVcbiAgICAgICAgLy8gZ2VuZXJhdGVkIHBvc2l0aW9ucyByZWxhdGl2ZSB0byB0aGUgc3RhcnQgb2YgdGhlIHNlY3Rpb24sIHNvIHdlXG4gICAgICAgIC8vIG5lZWQgdG8gb2Zmc2V0IHRoZW0gdG8gYmUgcmVsYXRpdmUgdG8gdGhlIHN0YXJ0IG9mIHRoZSBjb25jYXRlbmF0ZWRcbiAgICAgICAgLy8gZ2VuZXJhdGVkIGZpbGUuXG4gICAgICAgIHZhciBhZGp1c3RlZE1hcHBpbmcgPSB7XG4gICAgICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICAgICAgZ2VuZXJhdGVkTGluZTogbWFwcGluZy5nZW5lcmF0ZWRMaW5lICtcbiAgICAgICAgICAgIChzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRMaW5lIC0gMSksXG4gICAgICAgICAgZ2VuZXJhdGVkQ29sdW1uOiBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiArXG4gICAgICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSA9PT0gbWFwcGluZy5nZW5lcmF0ZWRMaW5lXG4gICAgICAgICAgICA/IHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZENvbHVtbiAtIDFcbiAgICAgICAgICAgIDogMCksXG4gICAgICAgICAgb3JpZ2luYWxMaW5lOiBtYXBwaW5nLm9yaWdpbmFsTGluZSxcbiAgICAgICAgICBvcmlnaW5hbENvbHVtbjogbWFwcGluZy5vcmlnaW5hbENvbHVtbixcbiAgICAgICAgICBuYW1lOiBuYW1lXG4gICAgICAgIH07XG5cbiAgICAgICAgdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzLnB1c2goYWRqdXN0ZWRNYXBwaW5nKTtcbiAgICAgICAgaWYgKHR5cGVvZiBhZGp1c3RlZE1hcHBpbmcub3JpZ2luYWxMaW5lID09PSAnbnVtYmVyJykge1xuICAgICAgICAgIHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzLnB1c2goYWRqdXN0ZWRNYXBwaW5nKTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH1cblxuICAgIHF1aWNrU29ydCh0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MsIHV0aWwuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQpO1xuICAgIHF1aWNrU29ydCh0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncywgdXRpbC5jb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyk7XG4gIH07XG5cbmV4cG9ydHMuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyID0gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyO1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi9saWIvc291cmNlLW1hcC1jb25zdW1lci5qc1xuLy8gbW9kdWxlIGlkID0gM1xuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbmV4cG9ydHMuR1JFQVRFU1RfTE9XRVJfQk9VTkQgPSAxO1xuZXhwb3J0cy5MRUFTVF9VUFBFUl9CT1VORCA9IDI7XG5cbi8qKlxuICogUmVjdXJzaXZlIGltcGxlbWVudGF0aW9uIG9mIGJpbmFyeSBzZWFyY2guXG4gKlxuICogQHBhcmFtIGFMb3cgSW5kaWNlcyBoZXJlIGFuZCBsb3dlciBkbyBub3QgY29udGFpbiB0aGUgbmVlZGxlLlxuICogQHBhcmFtIGFIaWdoIEluZGljZXMgaGVyZSBhbmQgaGlnaGVyIGRvIG5vdCBjb250YWluIHRoZSBuZWVkbGUuXG4gKiBAcGFyYW0gYU5lZWRsZSBUaGUgZWxlbWVudCBiZWluZyBzZWFyY2hlZCBmb3IuXG4gKiBAcGFyYW0gYUhheXN0YWNrIFRoZSBub24tZW1wdHkgYXJyYXkgYmVpbmcgc2VhcmNoZWQuXG4gKiBAcGFyYW0gYUNvbXBhcmUgRnVuY3Rpb24gd2hpY2ggdGFrZXMgdHdvIGVsZW1lbnRzIGFuZCByZXR1cm5zIC0xLCAwLCBvciAxLlxuICogQHBhcmFtIGFCaWFzIEVpdGhlciAnYmluYXJ5U2VhcmNoLkdSRUFURVNUX0xPV0VSX0JPVU5EJyBvclxuICogICAgICdiaW5hcnlTZWFyY2guTEVBU1RfVVBQRVJfQk9VTkQnLiBTcGVjaWZpZXMgd2hldGhlciB0byByZXR1cm4gdGhlXG4gKiAgICAgY2xvc2VzdCBlbGVtZW50IHRoYXQgaXMgc21hbGxlciB0aGFuIG9yIGdyZWF0ZXIgdGhhbiB0aGUgb25lIHdlIGFyZVxuICogICAgIHNlYXJjaGluZyBmb3IsIHJlc3BlY3RpdmVseSwgaWYgdGhlIGV4YWN0IGVsZW1lbnQgY2Fubm90IGJlIGZvdW5kLlxuICovXG5mdW5jdGlvbiByZWN1cnNpdmVTZWFyY2goYUxvdywgYUhpZ2gsIGFOZWVkbGUsIGFIYXlzdGFjaywgYUNvbXBhcmUsIGFCaWFzKSB7XG4gIC8vIFRoaXMgZnVuY3Rpb24gdGVybWluYXRlcyB3aGVuIG9uZSBvZiB0aGUgZm9sbG93aW5nIGlzIHRydWU6XG4gIC8vXG4gIC8vICAgMS4gV2UgZmluZCB0aGUgZXhhY3QgZWxlbWVudCB3ZSBhcmUgbG9va2luZyBmb3IuXG4gIC8vXG4gIC8vICAgMi4gV2UgZGlkIG5vdCBmaW5kIHRoZSBleGFjdCBlbGVtZW50LCBidXQgd2UgY2FuIHJldHVybiB0aGUgaW5kZXggb2ZcbiAgLy8gICAgICB0aGUgbmV4dC1jbG9zZXN0IGVsZW1lbnQuXG4gIC8vXG4gIC8vICAgMy4gV2UgZGlkIG5vdCBmaW5kIHRoZSBleGFjdCBlbGVtZW50LCBhbmQgdGhlcmUgaXMgbm8gbmV4dC1jbG9zZXN0XG4gIC8vICAgICAgZWxlbWVudCB0aGFuIHRoZSBvbmUgd2UgYXJlIHNlYXJjaGluZyBmb3IsIHNvIHdlIHJldHVybiAtMS5cbiAgdmFyIG1pZCA9IE1hdGguZmxvb3IoKGFIaWdoIC0gYUxvdykgLyAyKSArIGFMb3c7XG4gIHZhciBjbXAgPSBhQ29tcGFyZShhTmVlZGxlLCBhSGF5c3RhY2tbbWlkXSwgdHJ1ZSk7XG4gIGlmIChjbXAgPT09IDApIHtcbiAgICAvLyBGb3VuZCB0aGUgZWxlbWVudCB3ZSBhcmUgbG9va2luZyBmb3IuXG4gICAgcmV0dXJuIG1pZDtcbiAgfVxuICBlbHNlIGlmIChjbXAgPiAwKSB7XG4gICAgLy8gT3VyIG5lZWRsZSBpcyBncmVhdGVyIHRoYW4gYUhheXN0YWNrW21pZF0uXG4gICAgaWYgKGFIaWdoIC0gbWlkID4gMSkge1xuICAgICAgLy8gVGhlIGVsZW1lbnQgaXMgaW4gdGhlIHVwcGVyIGhhbGYuXG4gICAgICByZXR1cm4gcmVjdXJzaXZlU2VhcmNoKG1pZCwgYUhpZ2gsIGFOZWVkbGUsIGFIYXlzdGFjaywgYUNvbXBhcmUsIGFCaWFzKTtcbiAgICB9XG5cbiAgICAvLyBUaGUgZXhhY3QgbmVlZGxlIGVsZW1lbnQgd2FzIG5vdCBmb3VuZCBpbiB0aGlzIGhheXN0YWNrLiBEZXRlcm1pbmUgaWZcbiAgICAvLyB3ZSBhcmUgaW4gdGVybWluYXRpb24gY2FzZSAoMykgb3IgKDIpIGFuZCByZXR1cm4gdGhlIGFwcHJvcHJpYXRlIHRoaW5nLlxuICAgIGlmIChhQmlhcyA9PSBleHBvcnRzLkxFQVNUX1VQUEVSX0JPVU5EKSB7XG4gICAgICByZXR1cm4gYUhpZ2ggPCBhSGF5c3RhY2subGVuZ3RoID8gYUhpZ2ggOiAtMTtcbiAgICB9IGVsc2Uge1xuICAgICAgcmV0dXJuIG1pZDtcbiAgICB9XG4gIH1cbiAgZWxzZSB7XG4gICAgLy8gT3VyIG5lZWRsZSBpcyBsZXNzIHRoYW4gYUhheXN0YWNrW21pZF0uXG4gICAgaWYgKG1pZCAtIGFMb3cgPiAxKSB7XG4gICAgICAvLyBUaGUgZWxlbWVudCBpcyBpbiB0aGUgbG93ZXIgaGFsZi5cbiAgICAgIHJldHVybiByZWN1cnNpdmVTZWFyY2goYUxvdywgbWlkLCBhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcyk7XG4gICAgfVxuXG4gICAgLy8gd2UgYXJlIGluIHRlcm1pbmF0aW9uIGNhc2UgKDMpIG9yICgyKSBhbmQgcmV0dXJuIHRoZSBhcHByb3ByaWF0ZSB0aGluZy5cbiAgICBpZiAoYUJpYXMgPT0gZXhwb3J0cy5MRUFTVF9VUFBFUl9CT1VORCkge1xuICAgICAgcmV0dXJuIG1pZDtcbiAgICB9IGVsc2Uge1xuICAgICAgcmV0dXJuIGFMb3cgPCAwID8gLTEgOiBhTG93O1xuICAgIH1cbiAgfVxufVxuXG4vKipcbiAqIFRoaXMgaXMgYW4gaW1wbGVtZW50YXRpb24gb2YgYmluYXJ5IHNlYXJjaCB3aGljaCB3aWxsIGFsd2F5cyB0cnkgYW5kIHJldHVyblxuICogdGhlIGluZGV4IG9mIHRoZSBjbG9zZXN0IGVsZW1lbnQgaWYgdGhlcmUgaXMgbm8gZXhhY3QgaGl0LiBUaGlzIGlzIGJlY2F1c2VcbiAqIG1hcHBpbmdzIGJldHdlZW4gb3JpZ2luYWwgYW5kIGdlbmVyYXRlZCBsaW5lL2NvbCBwYWlycyBhcmUgc2luZ2xlIHBvaW50cyxcbiAqIGFuZCB0aGVyZSBpcyBhbiBpbXBsaWNpdCByZWdpb24gYmV0d2VlbiBlYWNoIG9mIHRoZW0sIHNvIGEgbWlzcyBqdXN0IG1lYW5zXG4gKiB0aGF0IHlvdSBhcmVuJ3Qgb24gdGhlIHZlcnkgc3RhcnQgb2YgYSByZWdpb24uXG4gKlxuICogQHBhcmFtIGFOZWVkbGUgVGhlIGVsZW1lbnQgeW91IGFyZSBsb29raW5nIGZvci5cbiAqIEBwYXJhbSBhSGF5c3RhY2sgVGhlIGFycmF5IHRoYXQgaXMgYmVpbmcgc2VhcmNoZWQuXG4gKiBAcGFyYW0gYUNvbXBhcmUgQSBmdW5jdGlvbiB3aGljaCB0YWtlcyB0aGUgbmVlZGxlIGFuZCBhbiBlbGVtZW50IGluIHRoZVxuICogICAgIGFycmF5IGFuZCByZXR1cm5zIC0xLCAwLCBvciAxIGRlcGVuZGluZyBvbiB3aGV0aGVyIHRoZSBuZWVkbGUgaXMgbGVzc1xuICogICAgIHRoYW4sIGVxdWFsIHRvLCBvciBncmVhdGVyIHRoYW4gdGhlIGVsZW1lbnQsIHJlc3BlY3RpdmVseS5cbiAqIEBwYXJhbSBhQmlhcyBFaXRoZXIgJ2JpbmFyeVNlYXJjaC5HUkVBVEVTVF9MT1dFUl9CT1VORCcgb3JcbiAqICAgICAnYmluYXJ5U2VhcmNoLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAqICAgICBzZWFyY2hpbmcgZm9yLCByZXNwZWN0aXZlbHksIGlmIHRoZSBleGFjdCBlbGVtZW50IGNhbm5vdCBiZSBmb3VuZC5cbiAqICAgICBEZWZhdWx0cyB0byAnYmluYXJ5U2VhcmNoLkdSRUFURVNUX0xPV0VSX0JPVU5EJy5cbiAqL1xuZXhwb3J0cy5zZWFyY2ggPSBmdW5jdGlvbiBzZWFyY2goYU5lZWRsZSwgYUhheXN0YWNrLCBhQ29tcGFyZSwgYUJpYXMpIHtcbiAgaWYgKGFIYXlzdGFjay5sZW5ndGggPT09IDApIHtcbiAgICByZXR1cm4gLTE7XG4gIH1cblxuICB2YXIgaW5kZXggPSByZWN1cnNpdmVTZWFyY2goLTEsIGFIYXlzdGFjay5sZW5ndGgsIGFOZWVkbGUsIGFIYXlzdGFjayxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGFDb21wYXJlLCBhQmlhcyB8fCBleHBvcnRzLkdSRUFURVNUX0xPV0VSX0JPVU5EKTtcbiAgaWYgKGluZGV4IDwgMCkge1xuICAgIHJldHVybiAtMTtcbiAgfVxuXG4gIC8vIFdlIGhhdmUgZm91bmQgZWl0aGVyIHRoZSBleGFjdCBlbGVtZW50LCBvciB0aGUgbmV4dC1jbG9zZXN0IGVsZW1lbnQgdGhhblxuICAvLyB0aGUgb25lIHdlIGFyZSBzZWFyY2hpbmcgZm9yLiBIb3dldmVyLCB0aGVyZSBtYXkgYmUgbW9yZSB0aGFuIG9uZSBzdWNoXG4gIC8vIGVsZW1lbnQuIE1ha2Ugc3VyZSB3ZSBhbHdheXMgcmV0dXJuIHRoZSBzbWFsbGVzdCBvZiB0aGVzZS5cbiAgd2hpbGUgKGluZGV4IC0gMSA+PSAwKSB7XG4gICAgaWYgKGFDb21wYXJlKGFIYXlzdGFja1tpbmRleF0sIGFIYXlzdGFja1tpbmRleCAtIDFdLCB0cnVlKSAhPT0gMCkge1xuICAgICAgYnJlYWs7XG4gICAgfVxuICAgIC0taW5kZXg7XG4gIH1cblxuICByZXR1cm4gaW5kZXg7XG59O1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi9saWIvYmluYXJ5LXNlYXJjaC5qc1xuLy8gbW9kdWxlIGlkID0gNFxuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbnZhciB1dGlsID0gcmVxdWlyZSgnLi91dGlsJyk7XG52YXIgaGFzID0gT2JqZWN0LnByb3RvdHlwZS5oYXNPd25Qcm9wZXJ0eTtcbnZhciBoYXNOYXRpdmVNYXAgPSB0eXBlb2YgTWFwICE9PSBcInVuZGVmaW5lZFwiO1xuXG4vKipcbiAqIEEgZGF0YSBzdHJ1Y3R1cmUgd2hpY2ggaXMgYSBjb21iaW5hdGlvbiBvZiBhbiBhcnJheSBhbmQgYSBzZXQuIEFkZGluZyBhIG5ld1xuICogbWVtYmVyIGlzIE8oMSksIHRlc3RpbmcgZm9yIG1lbWJlcnNoaXAgaXMgTygxKSwgYW5kIGZpbmRpbmcgdGhlIGluZGV4IG9mIGFuXG4gKiBlbGVtZW50IGlzIE8oMSkuIFJlbW92aW5nIGVsZW1lbnRzIGZyb20gdGhlIHNldCBpcyBub3Qgc3VwcG9ydGVkLiBPbmx5XG4gKiBzdHJpbmdzIGFyZSBzdXBwb3J0ZWQgZm9yIG1lbWJlcnNoaXAuXG4gKi9cbmZ1bmN0aW9uIEFycmF5U2V0KCkge1xuICB0aGlzLl9hcnJheSA9IFtdO1xuICB0aGlzLl9zZXQgPSBoYXNOYXRpdmVNYXAgPyBuZXcgTWFwKCkgOiBPYmplY3QuY3JlYXRlKG51bGwpO1xufVxuXG4vKipcbiAqIFN0YXRpYyBtZXRob2QgZm9yIGNyZWF0aW5nIEFycmF5U2V0IGluc3RhbmNlcyBmcm9tIGFuIGV4aXN0aW5nIGFycmF5LlxuICovXG5BcnJheVNldC5mcm9tQXJyYXkgPSBmdW5jdGlvbiBBcnJheVNldF9mcm9tQXJyYXkoYUFycmF5LCBhQWxsb3dEdXBsaWNhdGVzKSB7XG4gIHZhciBzZXQgPSBuZXcgQXJyYXlTZXQoKTtcbiAgZm9yICh2YXIgaSA9IDAsIGxlbiA9IGFBcnJheS5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgIHNldC5hZGQoYUFycmF5W2ldLCBhQWxsb3dEdXBsaWNhdGVzKTtcbiAgfVxuICByZXR1cm4gc2V0O1xufTtcblxuLyoqXG4gKiBSZXR1cm4gaG93IG1hbnkgdW5pcXVlIGl0ZW1zIGFyZSBpbiB0aGlzIEFycmF5U2V0LiBJZiBkdXBsaWNhdGVzIGhhdmUgYmVlblxuICogYWRkZWQsIHRoYW4gdGhvc2UgZG8gbm90IGNvdW50IHRvd2FyZHMgdGhlIHNpemUuXG4gKlxuICogQHJldHVybnMgTnVtYmVyXG4gKi9cbkFycmF5U2V0LnByb3RvdHlwZS5zaXplID0gZnVuY3Rpb24gQXJyYXlTZXRfc2l6ZSgpIHtcbiAgcmV0dXJuIGhhc05hdGl2ZU1hcCA/IHRoaXMuX3NldC5zaXplIDogT2JqZWN0LmdldE93blByb3BlcnR5TmFtZXModGhpcy5fc2V0KS5sZW5ndGg7XG59O1xuXG4vKipcbiAqIEFkZCB0aGUgZ2l2ZW4gc3RyaW5nIHRvIHRoaXMgc2V0LlxuICpcbiAqIEBwYXJhbSBTdHJpbmcgYVN0clxuICovXG5BcnJheVNldC5wcm90b3R5cGUuYWRkID0gZnVuY3Rpb24gQXJyYXlTZXRfYWRkKGFTdHIsIGFBbGxvd0R1cGxpY2F0ZXMpIHtcbiAgdmFyIHNTdHIgPSBoYXNOYXRpdmVNYXAgPyBhU3RyIDogdXRpbC50b1NldFN0cmluZyhhU3RyKTtcbiAgdmFyIGlzRHVwbGljYXRlID0gaGFzTmF0aXZlTWFwID8gdGhpcy5oYXMoYVN0cikgOiBoYXMuY2FsbCh0aGlzLl9zZXQsIHNTdHIpO1xuICB2YXIgaWR4ID0gdGhpcy5fYXJyYXkubGVuZ3RoO1xuICBpZiAoIWlzRHVwbGljYXRlIHx8IGFBbGxvd0R1cGxpY2F0ZXMpIHtcbiAgICB0aGlzLl9hcnJheS5wdXNoKGFTdHIpO1xuICB9XG4gIGlmICghaXNEdXBsaWNhdGUpIHtcbiAgICBpZiAoaGFzTmF0aXZlTWFwKSB7XG4gICAgICB0aGlzLl9zZXQuc2V0KGFTdHIsIGlkeCk7XG4gICAgfSBlbHNlIHtcbiAgICAgIHRoaXMuX3NldFtzU3RyXSA9IGlkeDtcbiAgICB9XG4gIH1cbn07XG5cbi8qKlxuICogSXMgdGhlIGdpdmVuIHN0cmluZyBhIG1lbWJlciBvZiB0aGlzIHNldD9cbiAqXG4gKiBAcGFyYW0gU3RyaW5nIGFTdHJcbiAqL1xuQXJyYXlTZXQucHJvdG90eXBlLmhhcyA9IGZ1bmN0aW9uIEFycmF5U2V0X2hhcyhhU3RyKSB7XG4gIGlmIChoYXNOYXRpdmVNYXApIHtcbiAgICByZXR1cm4gdGhpcy5fc2V0LmhhcyhhU3RyKTtcbiAgfSBlbHNlIHtcbiAgICB2YXIgc1N0ciA9IHV0aWwudG9TZXRTdHJpbmcoYVN0cik7XG4gICAgcmV0dXJuIGhhcy5jYWxsKHRoaXMuX3NldCwgc1N0cik7XG4gIH1cbn07XG5cbi8qKlxuICogV2hhdCBpcyB0aGUgaW5kZXggb2YgdGhlIGdpdmVuIHN0cmluZyBpbiB0aGUgYXJyYXk/XG4gKlxuICogQHBhcmFtIFN0cmluZyBhU3RyXG4gKi9cbkFycmF5U2V0LnByb3RvdHlwZS5pbmRleE9mID0gZnVuY3Rpb24gQXJyYXlTZXRfaW5kZXhPZihhU3RyKSB7XG4gIGlmIChoYXNOYXRpdmVNYXApIHtcbiAgICB2YXIgaWR4ID0gdGhpcy5fc2V0LmdldChhU3RyKTtcbiAgICBpZiAoaWR4ID49IDApIHtcbiAgICAgICAgcmV0dXJuIGlkeDtcbiAgICB9XG4gIH0gZWxzZSB7XG4gICAgdmFyIHNTdHIgPSB1dGlsLnRvU2V0U3RyaW5nKGFTdHIpO1xuICAgIGlmIChoYXMuY2FsbCh0aGlzLl9zZXQsIHNTdHIpKSB7XG4gICAgICByZXR1cm4gdGhpcy5fc2V0W3NTdHJdO1xuICAgIH1cbiAgfVxuXG4gIHRocm93IG5ldyBFcnJvcignXCInICsgYVN0ciArICdcIiBpcyBub3QgaW4gdGhlIHNldC4nKTtcbn07XG5cbi8qKlxuICogV2hhdCBpcyB0aGUgZWxlbWVudCBhdCB0aGUgZ2l2ZW4gaW5kZXg/XG4gKlxuICogQHBhcmFtIE51bWJlciBhSWR4XG4gKi9cbkFycmF5U2V0LnByb3RvdHlwZS5hdCA9IGZ1bmN0aW9uIEFycmF5U2V0X2F0KGFJZHgpIHtcbiAgaWYgKGFJZHggPj0gMCAmJiBhSWR4IDwgdGhpcy5fYXJyYXkubGVuZ3RoKSB7XG4gICAgcmV0dXJuIHRoaXMuX2FycmF5W2FJZHhdO1xuICB9XG4gIHRocm93IG5ldyBFcnJvcignTm8gZWxlbWVudCBpbmRleGVkIGJ5ICcgKyBhSWR4KTtcbn07XG5cbi8qKlxuICogUmV0dXJucyB0aGUgYXJyYXkgcmVwcmVzZW50YXRpb24gb2YgdGhpcyBzZXQgKHdoaWNoIGhhcyB0aGUgcHJvcGVyIGluZGljZXNcbiAqIGluZGljYXRlZCBieSBpbmRleE9mKS4gTm90ZSB0aGF0IHRoaXMgaXMgYSBjb3B5IG9mIHRoZSBpbnRlcm5hbCBhcnJheSB1c2VkXG4gKiBmb3Igc3RvcmluZyB0aGUgbWVtYmVycyBzbyB0aGF0IG5vIG9uZSBjYW4gbWVzcyB3aXRoIGludGVybmFsIHN0YXRlLlxuICovXG5BcnJheVNldC5wcm90b3R5cGUudG9BcnJheSA9IGZ1bmN0aW9uIEFycmF5U2V0X3RvQXJyYXkoKSB7XG4gIHJldHVybiB0aGlzLl9hcnJheS5zbGljZSgpO1xufTtcblxuZXhwb3J0cy5BcnJheVNldCA9IEFycmF5U2V0O1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi9saWIvYXJyYXktc2V0LmpzXG4vLyBtb2R1bGUgaWQgPSA1XG4vLyBtb2R1bGUgY2h1bmtzID0gMCIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKlxuICogQmFzZWQgb24gdGhlIEJhc2UgNjQgVkxRIGltcGxlbWVudGF0aW9uIGluIENsb3N1cmUgQ29tcGlsZXI6XG4gKiBodHRwczovL2NvZGUuZ29vZ2xlLmNvbS9wL2Nsb3N1cmUtY29tcGlsZXIvc291cmNlL2Jyb3dzZS90cnVuay9zcmMvY29tL2dvb2dsZS9kZWJ1Z2dpbmcvc291cmNlbWFwL0Jhc2U2NFZMUS5qYXZhXG4gKlxuICogQ29weXJpZ2h0IDIwMTEgVGhlIENsb3N1cmUgQ29tcGlsZXIgQXV0aG9ycy4gQWxsIHJpZ2h0cyByZXNlcnZlZC5cbiAqIFJlZGlzdHJpYnV0aW9uIGFuZCB1c2UgaW4gc291cmNlIGFuZCBiaW5hcnkgZm9ybXMsIHdpdGggb3Igd2l0aG91dFxuICogbW9kaWZpY2F0aW9uLCBhcmUgcGVybWl0dGVkIHByb3ZpZGVkIHRoYXQgdGhlIGZvbGxvd2luZyBjb25kaXRpb25zIGFyZVxuICogbWV0OlxuICpcbiAqICAqIFJlZGlzdHJpYnV0aW9ucyBvZiBzb3VyY2UgY29kZSBtdXN0IHJldGFpbiB0aGUgYWJvdmUgY29weXJpZ2h0XG4gKiAgICBub3RpY2UsIHRoaXMgbGlzdCBvZiBjb25kaXRpb25zIGFuZCB0aGUgZm9sbG93aW5nIGRpc2NsYWltZXIuXG4gKiAgKiBSZWRpc3RyaWJ1dGlvbnMgaW4gYmluYXJ5IGZvcm0gbXVzdCByZXByb2R1Y2UgdGhlIGFib3ZlXG4gKiAgICBjb3B5cmlnaHQgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZ1xuICogICAgZGlzY2xhaW1lciBpbiB0aGUgZG9jdW1lbnRhdGlvbiBhbmQvb3Igb3RoZXIgbWF0ZXJpYWxzIHByb3ZpZGVkXG4gKiAgICB3aXRoIHRoZSBkaXN0cmlidXRpb24uXG4gKiAgKiBOZWl0aGVyIHRoZSBuYW1lIG9mIEdvb2dsZSBJbmMuIG5vciB0aGUgbmFtZXMgb2YgaXRzXG4gKiAgICBjb250cmlidXRvcnMgbWF5IGJlIHVzZWQgdG8gZW5kb3JzZSBvciBwcm9tb3RlIHByb2R1Y3RzIGRlcml2ZWRcbiAqICAgIGZyb20gdGhpcyBzb2Z0d2FyZSB3aXRob3V0IHNwZWNpZmljIHByaW9yIHdyaXR0ZW4gcGVybWlzc2lvbi5cbiAqXG4gKiBUSElTIFNPRlRXQVJFIElTIFBST1ZJREVEIEJZIFRIRSBDT1BZUklHSFQgSE9MREVSUyBBTkQgQ09OVFJJQlVUT1JTXG4gKiBcIkFTIElTXCIgQU5EIEFOWSBFWFBSRVNTIE9SIElNUExJRUQgV0FSUkFOVElFUywgSU5DTFVESU5HLCBCVVQgTk9UXG4gKiBMSU1JVEVEIFRPLCBUSEUgSU1QTElFRCBXQVJSQU5USUVTIE9GIE1FUkNIQU5UQUJJTElUWSBBTkQgRklUTkVTUyBGT1JcbiAqIEEgUEFSVElDVUxBUiBQVVJQT1NFIEFSRSBESVNDTEFJTUVELiBJTiBOTyBFVkVOVCBTSEFMTCBUSEUgQ09QWVJJR0hUXG4gKiBPV05FUiBPUiBDT05UUklCVVRPUlMgQkUgTElBQkxFIEZPUiBBTlkgRElSRUNULCBJTkRJUkVDVCwgSU5DSURFTlRBTCxcbiAqIFNQRUNJQUwsIEVYRU1QTEFSWSwgT1IgQ09OU0VRVUVOVElBTCBEQU1BR0VTIChJTkNMVURJTkcsIEJVVCBOT1RcbiAqIExJTUlURUQgVE8sIFBST0NVUkVNRU5UIE9GIFNVQlNUSVRVVEUgR09PRFMgT1IgU0VSVklDRVM7IExPU1MgT0YgVVNFLFxuICogREFUQSwgT1IgUFJPRklUUzsgT1IgQlVTSU5FU1MgSU5URVJSVVBUSU9OKSBIT1dFVkVSIENBVVNFRCBBTkQgT04gQU5ZXG4gKiBUSEVPUlkgT0YgTElBQklMSVRZLCBXSEVUSEVSIElOIENPTlRSQUNULCBTVFJJQ1QgTElBQklMSVRZLCBPUiBUT1JUXG4gKiAoSU5DTFVESU5HIE5FR0xJR0VOQ0UgT1IgT1RIRVJXSVNFKSBBUklTSU5HIElOIEFOWSBXQVkgT1VUIE9GIFRIRSBVU0VcbiAqIE9GIFRISVMgU09GVFdBUkUsIEVWRU4gSUYgQURWSVNFRCBPRiBUSEUgUE9TU0lCSUxJVFkgT0YgU1VDSCBEQU1BR0UuXG4gKi9cblxudmFyIGJhc2U2NCA9IHJlcXVpcmUoJy4vYmFzZTY0Jyk7XG5cbi8vIEEgc2luZ2xlIGJhc2UgNjQgZGlnaXQgY2FuIGNvbnRhaW4gNiBiaXRzIG9mIGRhdGEuIEZvciB0aGUgYmFzZSA2NCB2YXJpYWJsZVxuLy8gbGVuZ3RoIHF1YW50aXRpZXMgd2UgdXNlIGluIHRoZSBzb3VyY2UgbWFwIHNwZWMsIHRoZSBmaXJzdCBiaXQgaXMgdGhlIHNpZ24sXG4vLyB0aGUgbmV4dCBmb3VyIGJpdHMgYXJlIHRoZSBhY3R1YWwgdmFsdWUsIGFuZCB0aGUgNnRoIGJpdCBpcyB0aGVcbi8vIGNvbnRpbnVhdGlvbiBiaXQuIFRoZSBjb250aW51YXRpb24gYml0IHRlbGxzIHVzIHdoZXRoZXIgdGhlcmUgYXJlIG1vcmVcbi8vIGRpZ2l0cyBpbiB0aGlzIHZhbHVlIGZvbGxvd2luZyB0aGlzIGRpZ2l0LlxuLy9cbi8vICAgQ29udGludWF0aW9uXG4vLyAgIHwgICAgU2lnblxuLy8gICB8ICAgIHxcbi8vICAgViAgICBWXG4vLyAgIDEwMTAxMVxuXG52YXIgVkxRX0JBU0VfU0hJRlQgPSA1O1xuXG4vLyBiaW5hcnk6IDEwMDAwMFxudmFyIFZMUV9CQVNFID0gMSA8PCBWTFFfQkFTRV9TSElGVDtcblxuLy8gYmluYXJ5OiAwMTExMTFcbnZhciBWTFFfQkFTRV9NQVNLID0gVkxRX0JBU0UgLSAxO1xuXG4vLyBiaW5hcnk6IDEwMDAwMFxudmFyIFZMUV9DT05USU5VQVRJT05fQklUID0gVkxRX0JBU0U7XG5cbi8qKlxuICogQ29udmVydHMgZnJvbSBhIHR3by1jb21wbGVtZW50IHZhbHVlIHRvIGEgdmFsdWUgd2hlcmUgdGhlIHNpZ24gYml0IGlzXG4gKiBwbGFjZWQgaW4gdGhlIGxlYXN0IHNpZ25pZmljYW50IGJpdC4gIEZvciBleGFtcGxlLCBhcyBkZWNpbWFsczpcbiAqICAgMSBiZWNvbWVzIDIgKDEwIGJpbmFyeSksIC0xIGJlY29tZXMgMyAoMTEgYmluYXJ5KVxuICogICAyIGJlY29tZXMgNCAoMTAwIGJpbmFyeSksIC0yIGJlY29tZXMgNSAoMTAxIGJpbmFyeSlcbiAqL1xuZnVuY3Rpb24gdG9WTFFTaWduZWQoYVZhbHVlKSB7XG4gIHJldHVybiBhVmFsdWUgPCAwXG4gICAgPyAoKC1hVmFsdWUpIDw8IDEpICsgMVxuICAgIDogKGFWYWx1ZSA8PCAxKSArIDA7XG59XG5cbi8qKlxuICogQ29udmVydHMgdG8gYSB0d28tY29tcGxlbWVudCB2YWx1ZSBmcm9tIGEgdmFsdWUgd2hlcmUgdGhlIHNpZ24gYml0IGlzXG4gKiBwbGFjZWQgaW4gdGhlIGxlYXN0IHNpZ25pZmljYW50IGJpdC4gIEZvciBleGFtcGxlLCBhcyBkZWNpbWFsczpcbiAqICAgMiAoMTAgYmluYXJ5KSBiZWNvbWVzIDEsIDMgKDExIGJpbmFyeSkgYmVjb21lcyAtMVxuICogICA0ICgxMDAgYmluYXJ5KSBiZWNvbWVzIDIsIDUgKDEwMSBiaW5hcnkpIGJlY29tZXMgLTJcbiAqL1xuZnVuY3Rpb24gZnJvbVZMUVNpZ25lZChhVmFsdWUpIHtcbiAgdmFyIGlzTmVnYXRpdmUgPSAoYVZhbHVlICYgMSkgPT09IDE7XG4gIHZhciBzaGlmdGVkID0gYVZhbHVlID4+IDE7XG4gIHJldHVybiBpc05lZ2F0aXZlXG4gICAgPyAtc2hpZnRlZFxuICAgIDogc2hpZnRlZDtcbn1cblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBiYXNlIDY0IFZMUSBlbmNvZGVkIHZhbHVlLlxuICovXG5leHBvcnRzLmVuY29kZSA9IGZ1bmN0aW9uIGJhc2U2NFZMUV9lbmNvZGUoYVZhbHVlKSB7XG4gIHZhciBlbmNvZGVkID0gXCJcIjtcbiAgdmFyIGRpZ2l0O1xuXG4gIHZhciB2bHEgPSB0b1ZMUVNpZ25lZChhVmFsdWUpO1xuXG4gIGRvIHtcbiAgICBkaWdpdCA9IHZscSAmIFZMUV9CQVNFX01BU0s7XG4gICAgdmxxID4+Pj0gVkxRX0JBU0VfU0hJRlQ7XG4gICAgaWYgKHZscSA+IDApIHtcbiAgICAgIC8vIFRoZXJlIGFyZSBzdGlsbCBtb3JlIGRpZ2l0cyBpbiB0aGlzIHZhbHVlLCBzbyB3ZSBtdXN0IG1ha2Ugc3VyZSB0aGVcbiAgICAgIC8vIGNvbnRpbnVhdGlvbiBiaXQgaXMgbWFya2VkLlxuICAgICAgZGlnaXQgfD0gVkxRX0NPTlRJTlVBVElPTl9CSVQ7XG4gICAgfVxuICAgIGVuY29kZWQgKz0gYmFzZTY0LmVuY29kZShkaWdpdCk7XG4gIH0gd2hpbGUgKHZscSA+IDApO1xuXG4gIHJldHVybiBlbmNvZGVkO1xufTtcblxuLyoqXG4gKiBEZWNvZGVzIHRoZSBuZXh0IGJhc2UgNjQgVkxRIHZhbHVlIGZyb20gdGhlIGdpdmVuIHN0cmluZyBhbmQgcmV0dXJucyB0aGVcbiAqIHZhbHVlIGFuZCB0aGUgcmVzdCBvZiB0aGUgc3RyaW5nIHZpYSB0aGUgb3V0IHBhcmFtZXRlci5cbiAqL1xuZXhwb3J0cy5kZWNvZGUgPSBmdW5jdGlvbiBiYXNlNjRWTFFfZGVjb2RlKGFTdHIsIGFJbmRleCwgYU91dFBhcmFtKSB7XG4gIHZhciBzdHJMZW4gPSBhU3RyLmxlbmd0aDtcbiAgdmFyIHJlc3VsdCA9IDA7XG4gIHZhciBzaGlmdCA9IDA7XG4gIHZhciBjb250aW51YXRpb24sIGRpZ2l0O1xuXG4gIGRvIHtcbiAgICBpZiAoYUluZGV4ID49IHN0ckxlbikge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKFwiRXhwZWN0ZWQgbW9yZSBkaWdpdHMgaW4gYmFzZSA2NCBWTFEgdmFsdWUuXCIpO1xuICAgIH1cblxuICAgIGRpZ2l0ID0gYmFzZTY0LmRlY29kZShhU3RyLmNoYXJDb2RlQXQoYUluZGV4KyspKTtcbiAgICBpZiAoZGlnaXQgPT09IC0xKSB7XG4gICAgICB0aHJvdyBuZXcgRXJyb3IoXCJJbnZhbGlkIGJhc2U2NCBkaWdpdDogXCIgKyBhU3RyLmNoYXJBdChhSW5kZXggLSAxKSk7XG4gICAgfVxuXG4gICAgY29udGludWF0aW9uID0gISEoZGlnaXQgJiBWTFFfQ09OVElOVUFUSU9OX0JJVCk7XG4gICAgZGlnaXQgJj0gVkxRX0JBU0VfTUFTSztcbiAgICByZXN1bHQgPSByZXN1bHQgKyAoZGlnaXQgPDwgc2hpZnQpO1xuICAgIHNoaWZ0ICs9IFZMUV9CQVNFX1NISUZUO1xuICB9IHdoaWxlIChjb250aW51YXRpb24pO1xuXG4gIGFPdXRQYXJhbS52YWx1ZSA9IGZyb21WTFFTaWduZWQocmVzdWx0KTtcbiAgYU91dFBhcmFtLnJlc3QgPSBhSW5kZXg7XG59O1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi9saWIvYmFzZTY0LXZscS5qc1xuLy8gbW9kdWxlIGlkID0gNlxuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbnZhciBpbnRUb0NoYXJNYXAgPSAnQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ejAxMjM0NTY3ODkrLycuc3BsaXQoJycpO1xuXG4vKipcbiAqIEVuY29kZSBhbiBpbnRlZ2VyIGluIHRoZSByYW5nZSBvZiAwIHRvIDYzIHRvIGEgc2luZ2xlIGJhc2UgNjQgZGlnaXQuXG4gKi9cbmV4cG9ydHMuZW5jb2RlID0gZnVuY3Rpb24gKG51bWJlcikge1xuICBpZiAoMCA8PSBudW1iZXIgJiYgbnVtYmVyIDwgaW50VG9DaGFyTWFwLmxlbmd0aCkge1xuICAgIHJldHVybiBpbnRUb0NoYXJNYXBbbnVtYmVyXTtcbiAgfVxuICB0aHJvdyBuZXcgVHlwZUVycm9yKFwiTXVzdCBiZSBiZXR3ZWVuIDAgYW5kIDYzOiBcIiArIG51bWJlcik7XG59O1xuXG4vKipcbiAqIERlY29kZSBhIHNpbmdsZSBiYXNlIDY0IGNoYXJhY3RlciBjb2RlIGRpZ2l0IHRvIGFuIGludGVnZXIuIFJldHVybnMgLTEgb25cbiAqIGZhaWx1cmUuXG4gKi9cbmV4cG9ydHMuZGVjb2RlID0gZnVuY3Rpb24gKGNoYXJDb2RlKSB7XG4gIHZhciBiaWdBID0gNjU7ICAgICAvLyAnQSdcbiAgdmFyIGJpZ1ogPSA5MDsgICAgIC8vICdaJ1xuXG4gIHZhciBsaXR0bGVBID0gOTc7ICAvLyAnYSdcbiAgdmFyIGxpdHRsZVogPSAxMjI7IC8vICd6J1xuXG4gIHZhciB6ZXJvID0gNDg7ICAgICAvLyAnMCdcbiAgdmFyIG5pbmUgPSA1NzsgICAgIC8vICc5J1xuXG4gIHZhciBwbHVzID0gNDM7ICAgICAvLyAnKydcbiAgdmFyIHNsYXNoID0gNDc7ICAgIC8vICcvJ1xuXG4gIHZhciBsaXR0bGVPZmZzZXQgPSAyNjtcbiAgdmFyIG51bWJlck9mZnNldCA9IDUyO1xuXG4gIC8vIDAgLSAyNTogQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpcbiAgaWYgKGJpZ0EgPD0gY2hhckNvZGUgJiYgY2hhckNvZGUgPD0gYmlnWikge1xuICAgIHJldHVybiAoY2hhckNvZGUgLSBiaWdBKTtcbiAgfVxuXG4gIC8vIDI2IC0gNTE6IGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6XG4gIGlmIChsaXR0bGVBIDw9IGNoYXJDb2RlICYmIGNoYXJDb2RlIDw9IGxpdHRsZVopIHtcbiAgICByZXR1cm4gKGNoYXJDb2RlIC0gbGl0dGxlQSArIGxpdHRsZU9mZnNldCk7XG4gIH1cblxuICAvLyA1MiAtIDYxOiAwMTIzNDU2Nzg5XG4gIGlmICh6ZXJvIDw9IGNoYXJDb2RlICYmIGNoYXJDb2RlIDw9IG5pbmUpIHtcbiAgICByZXR1cm4gKGNoYXJDb2RlIC0gemVybyArIG51bWJlck9mZnNldCk7XG4gIH1cblxuICAvLyA2MjogK1xuICBpZiAoY2hhckNvZGUgPT0gcGx1cykge1xuICAgIHJldHVybiA2MjtcbiAgfVxuXG4gIC8vIDYzOiAvXG4gIGlmIChjaGFyQ29kZSA9PSBzbGFzaCkge1xuICAgIHJldHVybiA2MztcbiAgfVxuXG4gIC8vIEludmFsaWQgYmFzZTY0IGRpZ2l0LlxuICByZXR1cm4gLTE7XG59O1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi9saWIvYmFzZTY0LmpzXG4vLyBtb2R1bGUgaWQgPSA3XG4vLyBtb2R1bGUgY2h1bmtzID0gMCIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxuLy8gSXQgdHVybnMgb3V0IHRoYXQgc29tZSAobW9zdD8pIEphdmFTY3JpcHQgZW5naW5lcyBkb24ndCBzZWxmLWhvc3Rcbi8vIGBBcnJheS5wcm90b3R5cGUuc29ydGAuIFRoaXMgbWFrZXMgc2Vuc2UgYmVjYXVzZSBDKysgd2lsbCBsaWtlbHkgcmVtYWluXG4vLyBmYXN0ZXIgdGhhbiBKUyB3aGVuIGRvaW5nIHJhdyBDUFUtaW50ZW5zaXZlIHNvcnRpbmcuIEhvd2V2ZXIsIHdoZW4gdXNpbmcgYVxuLy8gY3VzdG9tIGNvbXBhcmF0b3IgZnVuY3Rpb24sIGNhbGxpbmcgYmFjayBhbmQgZm9ydGggYmV0d2VlbiB0aGUgVk0ncyBDKysgYW5kXG4vLyBKSVQnZCBKUyBpcyByYXRoZXIgc2xvdyAqYW5kKiBsb3NlcyBKSVQgdHlwZSBpbmZvcm1hdGlvbiwgcmVzdWx0aW5nIGluXG4vLyB3b3JzZSBnZW5lcmF0ZWQgY29kZSBmb3IgdGhlIGNvbXBhcmF0b3IgZnVuY3Rpb24gdGhhbiB3b3VsZCBiZSBvcHRpbWFsLiBJblxuLy8gZmFjdCwgd2hlbiBzb3J0aW5nIHdpdGggYSBjb21wYXJhdG9yLCB0aGVzZSBjb3N0cyBvdXR3ZWlnaCB0aGUgYmVuZWZpdHMgb2Zcbi8vIHNvcnRpbmcgaW4gQysrLiBCeSB1c2luZyBvdXIgb3duIEpTLWltcGxlbWVudGVkIFF1aWNrIFNvcnQgKGJlbG93KSwgd2UgZ2V0XG4vLyBhIH4zNTAwbXMgbWVhbiBzcGVlZC11cCBpbiBgYmVuY2gvYmVuY2guaHRtbGAuXG5cbi8qKlxuICogU3dhcCB0aGUgZWxlbWVudHMgaW5kZXhlZCBieSBgeGAgYW5kIGB5YCBpbiB0aGUgYXJyYXkgYGFyeWAuXG4gKlxuICogQHBhcmFtIHtBcnJheX0gYXJ5XG4gKiAgICAgICAgVGhlIGFycmF5LlxuICogQHBhcmFtIHtOdW1iZXJ9IHhcbiAqICAgICAgICBUaGUgaW5kZXggb2YgdGhlIGZpcnN0IGl0ZW0uXG4gKiBAcGFyYW0ge051bWJlcn0geVxuICogICAgICAgIFRoZSBpbmRleCBvZiB0aGUgc2Vjb25kIGl0ZW0uXG4gKi9cbmZ1bmN0aW9uIHN3YXAoYXJ5LCB4LCB5KSB7XG4gIHZhciB0ZW1wID0gYXJ5W3hdO1xuICBhcnlbeF0gPSBhcnlbeV07XG4gIGFyeVt5XSA9IHRlbXA7XG59XG5cbi8qKlxuICogUmV0dXJucyBhIHJhbmRvbSBpbnRlZ2VyIHdpdGhpbiB0aGUgcmFuZ2UgYGxvdyAuLiBoaWdoYCBpbmNsdXNpdmUuXG4gKlxuICogQHBhcmFtIHtOdW1iZXJ9IGxvd1xuICogICAgICAgIFRoZSBsb3dlciBib3VuZCBvbiB0aGUgcmFuZ2UuXG4gKiBAcGFyYW0ge051bWJlcn0gaGlnaFxuICogICAgICAgIFRoZSB1cHBlciBib3VuZCBvbiB0aGUgcmFuZ2UuXG4gKi9cbmZ1bmN0aW9uIHJhbmRvbUludEluUmFuZ2UobG93LCBoaWdoKSB7XG4gIHJldHVybiBNYXRoLnJvdW5kKGxvdyArIChNYXRoLnJhbmRvbSgpICogKGhpZ2ggLSBsb3cpKSk7XG59XG5cbi8qKlxuICogVGhlIFF1aWNrIFNvcnQgYWxnb3JpdGhtLlxuICpcbiAqIEBwYXJhbSB7QXJyYXl9IGFyeVxuICogICAgICAgIEFuIGFycmF5IHRvIHNvcnQuXG4gKiBAcGFyYW0ge2Z1bmN0aW9ufSBjb21wYXJhdG9yXG4gKiAgICAgICAgRnVuY3Rpb24gdG8gdXNlIHRvIGNvbXBhcmUgdHdvIGl0ZW1zLlxuICogQHBhcmFtIHtOdW1iZXJ9IHBcbiAqICAgICAgICBTdGFydCBpbmRleCBvZiB0aGUgYXJyYXlcbiAqIEBwYXJhbSB7TnVtYmVyfSByXG4gKiAgICAgICAgRW5kIGluZGV4IG9mIHRoZSBhcnJheVxuICovXG5mdW5jdGlvbiBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHAsIHIpIHtcbiAgLy8gSWYgb3VyIGxvd2VyIGJvdW5kIGlzIGxlc3MgdGhhbiBvdXIgdXBwZXIgYm91bmQsIHdlICgxKSBwYXJ0aXRpb24gdGhlXG4gIC8vIGFycmF5IGludG8gdHdvIHBpZWNlcyBhbmQgKDIpIHJlY3Vyc2Ugb24gZWFjaCBoYWxmLiBJZiBpdCBpcyBub3QsIHRoaXMgaXNcbiAgLy8gdGhlIGVtcHR5IGFycmF5IGFuZCBvdXIgYmFzZSBjYXNlLlxuXG4gIGlmIChwIDwgcikge1xuICAgIC8vICgxKSBQYXJ0aXRpb25pbmcuXG4gICAgLy9cbiAgICAvLyBUaGUgcGFydGl0aW9uaW5nIGNob29zZXMgYSBwaXZvdCBiZXR3ZWVuIGBwYCBhbmQgYHJgIGFuZCBtb3ZlcyBhbGxcbiAgICAvLyBlbGVtZW50cyB0aGF0IGFyZSBsZXNzIHRoYW4gb3IgZXF1YWwgdG8gdGhlIHBpdm90IHRvIHRoZSBiZWZvcmUgaXQsIGFuZFxuICAgIC8vIGFsbCB0aGUgZWxlbWVudHMgdGhhdCBhcmUgZ3JlYXRlciB0aGFuIGl0IGFmdGVyIGl0LiBUaGUgZWZmZWN0IGlzIHRoYXRcbiAgICAvLyBvbmNlIHBhcnRpdGlvbiBpcyBkb25lLCB0aGUgcGl2b3QgaXMgaW4gdGhlIGV4YWN0IHBsYWNlIGl0IHdpbGwgYmUgd2hlblxuICAgIC8vIHRoZSBhcnJheSBpcyBwdXQgaW4gc29ydGVkIG9yZGVyLCBhbmQgaXQgd2lsbCBub3QgbmVlZCB0byBiZSBtb3ZlZFxuICAgIC8vIGFnYWluLiBUaGlzIHJ1bnMgaW4gTyhuKSB0aW1lLlxuXG4gICAgLy8gQWx3YXlzIGNob29zZSBhIHJhbmRvbSBwaXZvdCBzbyB0aGF0IGFuIGlucHV0IGFycmF5IHdoaWNoIGlzIHJldmVyc2VcbiAgICAvLyBzb3J0ZWQgZG9lcyBub3QgY2F1c2UgTyhuXjIpIHJ1bm5pbmcgdGltZS5cbiAgICB2YXIgcGl2b3RJbmRleCA9IHJhbmRvbUludEluUmFuZ2UocCwgcik7XG4gICAgdmFyIGkgPSBwIC0gMTtcblxuICAgIHN3YXAoYXJ5LCBwaXZvdEluZGV4LCByKTtcbiAgICB2YXIgcGl2b3QgPSBhcnlbcl07XG5cbiAgICAvLyBJbW1lZGlhdGVseSBhZnRlciBgamAgaXMgaW5jcmVtZW50ZWQgaW4gdGhpcyBsb29wLCB0aGUgZm9sbG93aW5nIGhvbGRcbiAgICAvLyB0cnVlOlxuICAgIC8vXG4gICAgLy8gICAqIEV2ZXJ5IGVsZW1lbnQgaW4gYGFyeVtwIC4uIGldYCBpcyBsZXNzIHRoYW4gb3IgZXF1YWwgdG8gdGhlIHBpdm90LlxuICAgIC8vXG4gICAgLy8gICAqIEV2ZXJ5IGVsZW1lbnQgaW4gYGFyeVtpKzEgLi4gai0xXWAgaXMgZ3JlYXRlciB0aGFuIHRoZSBwaXZvdC5cbiAgICBmb3IgKHZhciBqID0gcDsgaiA8IHI7IGorKykge1xuICAgICAgaWYgKGNvbXBhcmF0b3IoYXJ5W2pdLCBwaXZvdCkgPD0gMCkge1xuICAgICAgICBpICs9IDE7XG4gICAgICAgIHN3YXAoYXJ5LCBpLCBqKTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICBzd2FwKGFyeSwgaSArIDEsIGopO1xuICAgIHZhciBxID0gaSArIDE7XG5cbiAgICAvLyAoMikgUmVjdXJzZSBvbiBlYWNoIGhhbGYuXG5cbiAgICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHAsIHEgLSAxKTtcbiAgICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHEgKyAxLCByKTtcbiAgfVxufVxuXG4vKipcbiAqIFNvcnQgdGhlIGdpdmVuIGFycmF5IGluLXBsYWNlIHdpdGggdGhlIGdpdmVuIGNvbXBhcmF0b3IgZnVuY3Rpb24uXG4gKlxuICogQHBhcmFtIHtBcnJheX0gYXJ5XG4gKiAgICAgICAgQW4gYXJyYXkgdG8gc29ydC5cbiAqIEBwYXJhbSB7ZnVuY3Rpb259IGNvbXBhcmF0b3JcbiAqICAgICAgICBGdW5jdGlvbiB0byB1c2UgdG8gY29tcGFyZSB0d28gaXRlbXMuXG4gKi9cbmV4cG9ydHMucXVpY2tTb3J0ID0gZnVuY3Rpb24gKGFyeSwgY29tcGFyYXRvcikge1xuICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIDAsIGFyeS5sZW5ndGggLSAxKTtcbn07XG5cblxuXG4vLy8vLy8vLy8vLy8vLy8vLy9cbi8vIFdFQlBBQ0sgRk9PVEVSXG4vLyAuL2xpYi9xdWljay1zb3J0LmpzXG4vLyBtb2R1bGUgaWQgPSA4XG4vLyBtb2R1bGUgY2h1bmtzID0gMCIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxudmFyIGJhc2U2NFZMUSA9IHJlcXVpcmUoJy4vYmFzZTY0LXZscScpO1xudmFyIHV0aWwgPSByZXF1aXJlKCcuL3V0aWwnKTtcbnZhciBBcnJheVNldCA9IHJlcXVpcmUoJy4vYXJyYXktc2V0JykuQXJyYXlTZXQ7XG52YXIgTWFwcGluZ0xpc3QgPSByZXF1aXJlKCcuL21hcHBpbmctbGlzdCcpLk1hcHBpbmdMaXN0O1xuXG4vKipcbiAqIEFuIGluc3RhbmNlIG9mIHRoZSBTb3VyY2VNYXBHZW5lcmF0b3IgcmVwcmVzZW50cyBhIHNvdXJjZSBtYXAgd2hpY2ggaXNcbiAqIGJlaW5nIGJ1aWx0IGluY3JlbWVudGFsbHkuIFlvdSBtYXkgcGFzcyBhbiBvYmplY3Qgd2l0aCB0aGUgZm9sbG93aW5nXG4gKiBwcm9wZXJ0aWVzOlxuICpcbiAqICAgLSBmaWxlOiBUaGUgZmlsZW5hbWUgb2YgdGhlIGdlbmVyYXRlZCBzb3VyY2UuXG4gKiAgIC0gc291cmNlUm9vdDogQSByb290IGZvciBhbGwgcmVsYXRpdmUgVVJMcyBpbiB0aGlzIHNvdXJjZSBtYXAuXG4gKi9cbmZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcihhQXJncykge1xuICBpZiAoIWFBcmdzKSB7XG4gICAgYUFyZ3MgPSB7fTtcbiAgfVxuICB0aGlzLl9maWxlID0gdXRpbC5nZXRBcmcoYUFyZ3MsICdmaWxlJywgbnVsbCk7XG4gIHRoaXMuX3NvdXJjZVJvb3QgPSB1dGlsLmdldEFyZyhhQXJncywgJ3NvdXJjZVJvb3QnLCBudWxsKTtcbiAgdGhpcy5fc2tpcFZhbGlkYXRpb24gPSB1dGlsLmdldEFyZyhhQXJncywgJ3NraXBWYWxpZGF0aW9uJywgZmFsc2UpO1xuICB0aGlzLl9zb3VyY2VzID0gbmV3IEFycmF5U2V0KCk7XG4gIHRoaXMuX25hbWVzID0gbmV3IEFycmF5U2V0KCk7XG4gIHRoaXMuX21hcHBpbmdzID0gbmV3IE1hcHBpbmdMaXN0KCk7XG4gIHRoaXMuX3NvdXJjZXNDb250ZW50cyA9IG51bGw7XG59XG5cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuX3ZlcnNpb24gPSAzO1xuXG4vKipcbiAqIENyZWF0ZXMgYSBuZXcgU291cmNlTWFwR2VuZXJhdG9yIGJhc2VkIG9uIGEgU291cmNlTWFwQ29uc3VtZXJcbiAqXG4gKiBAcGFyYW0gYVNvdXJjZU1hcENvbnN1bWVyIFRoZSBTb3VyY2VNYXAuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5mcm9tU291cmNlTWFwID1cbiAgZnVuY3Rpb24gU291cmNlTWFwR2VuZXJhdG9yX2Zyb21Tb3VyY2VNYXAoYVNvdXJjZU1hcENvbnN1bWVyKSB7XG4gICAgdmFyIHNvdXJjZVJvb3QgPSBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlUm9vdDtcbiAgICB2YXIgZ2VuZXJhdG9yID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgICBmaWxlOiBhU291cmNlTWFwQ29uc3VtZXIuZmlsZSxcbiAgICAgIHNvdXJjZVJvb3Q6IHNvdXJjZVJvb3RcbiAgICB9KTtcbiAgICBhU291cmNlTWFwQ29uc3VtZXIuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICAgIHZhciBuZXdNYXBwaW5nID0ge1xuICAgICAgICBnZW5lcmF0ZWQ6IHtcbiAgICAgICAgICBsaW5lOiBtYXBwaW5nLmdlbmVyYXRlZExpbmUsXG4gICAgICAgICAgY29sdW1uOiBtYXBwaW5nLmdlbmVyYXRlZENvbHVtblxuICAgICAgICB9XG4gICAgICB9O1xuXG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgIT0gbnVsbCkge1xuICAgICAgICBuZXdNYXBwaW5nLnNvdXJjZSA9IG1hcHBpbmcuc291cmNlO1xuICAgICAgICBpZiAoc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICAgICAgbmV3TWFwcGluZy5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIG5ld01hcHBpbmcuc291cmNlKTtcbiAgICAgICAgfVxuXG4gICAgICAgIG5ld01hcHBpbmcub3JpZ2luYWwgPSB7XG4gICAgICAgICAgbGluZTogbWFwcGluZy5vcmlnaW5hbExpbmUsXG4gICAgICAgICAgY29sdW1uOiBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uXG4gICAgICAgIH07XG5cbiAgICAgICAgaWYgKG1hcHBpbmcubmFtZSAhPSBudWxsKSB7XG4gICAgICAgICAgbmV3TWFwcGluZy5uYW1lID0gbWFwcGluZy5uYW1lO1xuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIGdlbmVyYXRvci5hZGRNYXBwaW5nKG5ld01hcHBpbmcpO1xuICAgIH0pO1xuICAgIGFTb3VyY2VNYXBDb25zdW1lci5zb3VyY2VzLmZvckVhY2goZnVuY3Rpb24gKHNvdXJjZUZpbGUpIHtcbiAgICAgIHZhciBzb3VyY2VSZWxhdGl2ZSA9IHNvdXJjZUZpbGU7XG4gICAgICBpZiAoc291cmNlUm9vdCAhPT0gbnVsbCkge1xuICAgICAgICBzb3VyY2VSZWxhdGl2ZSA9IHV0aWwucmVsYXRpdmUoc291cmNlUm9vdCwgc291cmNlRmlsZSk7XG4gICAgICB9XG5cbiAgICAgIGlmICghZ2VuZXJhdG9yLl9zb3VyY2VzLmhhcyhzb3VyY2VSZWxhdGl2ZSkpIHtcbiAgICAgICAgZ2VuZXJhdG9yLl9zb3VyY2VzLmFkZChzb3VyY2VSZWxhdGl2ZSk7XG4gICAgICB9XG5cbiAgICAgIHZhciBjb250ZW50ID0gYVNvdXJjZU1hcENvbnN1bWVyLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlRmlsZSk7XG4gICAgICBpZiAoY29udGVudCAhPSBudWxsKSB7XG4gICAgICAgIGdlbmVyYXRvci5zZXRTb3VyY2VDb250ZW50KHNvdXJjZUZpbGUsIGNvbnRlbnQpO1xuICAgICAgfVxuICAgIH0pO1xuICAgIHJldHVybiBnZW5lcmF0b3I7XG4gIH07XG5cbi8qKlxuICogQWRkIGEgc2luZ2xlIG1hcHBpbmcgZnJvbSBvcmlnaW5hbCBzb3VyY2UgbGluZSBhbmQgY29sdW1uIHRvIHRoZSBnZW5lcmF0ZWRcbiAqIHNvdXJjZSdzIGxpbmUgYW5kIGNvbHVtbiBmb3IgdGhpcyBzb3VyY2UgbWFwIGJlaW5nIGNyZWF0ZWQuIFRoZSBtYXBwaW5nXG4gKiBvYmplY3Qgc2hvdWxkIGhhdmUgdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICpcbiAqICAgLSBnZW5lcmF0ZWQ6IEFuIG9iamVjdCB3aXRoIHRoZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uIHBvc2l0aW9ucy5cbiAqICAgLSBvcmlnaW5hbDogQW4gb2JqZWN0IHdpdGggdGhlIG9yaWdpbmFsIGxpbmUgYW5kIGNvbHVtbiBwb3NpdGlvbnMuXG4gKiAgIC0gc291cmNlOiBUaGUgb3JpZ2luYWwgc291cmNlIGZpbGUgKHJlbGF0aXZlIHRvIHRoZSBzb3VyY2VSb290KS5cbiAqICAgLSBuYW1lOiBBbiBvcHRpb25hbCBvcmlnaW5hbCB0b2tlbiBuYW1lIGZvciB0aGlzIG1hcHBpbmcuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYWRkTWFwcGluZyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9hZGRNYXBwaW5nKGFBcmdzKSB7XG4gICAgdmFyIGdlbmVyYXRlZCA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnZ2VuZXJhdGVkJyk7XG4gICAgdmFyIG9yaWdpbmFsID0gdXRpbC5nZXRBcmcoYUFyZ3MsICdvcmlnaW5hbCcsIG51bGwpO1xuICAgIHZhciBzb3VyY2UgPSB1dGlsLmdldEFyZyhhQXJncywgJ3NvdXJjZScsIG51bGwpO1xuICAgIHZhciBuYW1lID0gdXRpbC5nZXRBcmcoYUFyZ3MsICduYW1lJywgbnVsbCk7XG5cbiAgICBpZiAoIXRoaXMuX3NraXBWYWxpZGF0aW9uKSB7XG4gICAgICB0aGlzLl92YWxpZGF0ZU1hcHBpbmcoZ2VuZXJhdGVkLCBvcmlnaW5hbCwgc291cmNlLCBuYW1lKTtcbiAgICB9XG5cbiAgICBpZiAoc291cmNlICE9IG51bGwpIHtcbiAgICAgIHNvdXJjZSA9IFN0cmluZyhzb3VyY2UpO1xuICAgICAgaWYgKCF0aGlzLl9zb3VyY2VzLmhhcyhzb3VyY2UpKSB7XG4gICAgICAgIHRoaXMuX3NvdXJjZXMuYWRkKHNvdXJjZSk7XG4gICAgICB9XG4gICAgfVxuXG4gICAgaWYgKG5hbWUgIT0gbnVsbCkge1xuICAgICAgbmFtZSA9IFN0cmluZyhuYW1lKTtcbiAgICAgIGlmICghdGhpcy5fbmFtZXMuaGFzKG5hbWUpKSB7XG4gICAgICAgIHRoaXMuX25hbWVzLmFkZChuYW1lKTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICB0aGlzLl9tYXBwaW5ncy5hZGQoe1xuICAgICAgZ2VuZXJhdGVkTGluZTogZ2VuZXJhdGVkLmxpbmUsXG4gICAgICBnZW5lcmF0ZWRDb2x1bW46IGdlbmVyYXRlZC5jb2x1bW4sXG4gICAgICBvcmlnaW5hbExpbmU6IG9yaWdpbmFsICE9IG51bGwgJiYgb3JpZ2luYWwubGluZSxcbiAgICAgIG9yaWdpbmFsQ29sdW1uOiBvcmlnaW5hbCAhPSBudWxsICYmIG9yaWdpbmFsLmNvbHVtbixcbiAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgbmFtZTogbmFtZVxuICAgIH0pO1xuICB9O1xuXG4vKipcbiAqIFNldCB0aGUgc291cmNlIGNvbnRlbnQgZm9yIGEgc291cmNlIGZpbGUuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuc2V0U291cmNlQ29udGVudCA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9zZXRTb3VyY2VDb250ZW50KGFTb3VyY2VGaWxlLCBhU291cmNlQ29udGVudCkge1xuICAgIHZhciBzb3VyY2UgPSBhU291cmNlRmlsZTtcbiAgICBpZiAodGhpcy5fc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICBzb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHRoaXMuX3NvdXJjZVJvb3QsIHNvdXJjZSk7XG4gICAgfVxuXG4gICAgaWYgKGFTb3VyY2VDb250ZW50ICE9IG51bGwpIHtcbiAgICAgIC8vIEFkZCB0aGUgc291cmNlIGNvbnRlbnQgdG8gdGhlIF9zb3VyY2VzQ29udGVudHMgbWFwLlxuICAgICAgLy8gQ3JlYXRlIGEgbmV3IF9zb3VyY2VzQ29udGVudHMgbWFwIGlmIHRoZSBwcm9wZXJ0eSBpcyBudWxsLlxuICAgICAgaWYgKCF0aGlzLl9zb3VyY2VzQ29udGVudHMpIHtcbiAgICAgICAgdGhpcy5fc291cmNlc0NvbnRlbnRzID0gT2JqZWN0LmNyZWF0ZShudWxsKTtcbiAgICAgIH1cbiAgICAgIHRoaXMuX3NvdXJjZXNDb250ZW50c1t1dGlsLnRvU2V0U3RyaW5nKHNvdXJjZSldID0gYVNvdXJjZUNvbnRlbnQ7XG4gICAgfSBlbHNlIGlmICh0aGlzLl9zb3VyY2VzQ29udGVudHMpIHtcbiAgICAgIC8vIFJlbW92ZSB0aGUgc291cmNlIGZpbGUgZnJvbSB0aGUgX3NvdXJjZXNDb250ZW50cyBtYXAuXG4gICAgICAvLyBJZiB0aGUgX3NvdXJjZXNDb250ZW50cyBtYXAgaXMgZW1wdHksIHNldCB0aGUgcHJvcGVydHkgdG8gbnVsbC5cbiAgICAgIGRlbGV0ZSB0aGlzLl9zb3VyY2VzQ29udGVudHNbdXRpbC50b1NldFN0cmluZyhzb3VyY2UpXTtcbiAgICAgIGlmIChPYmplY3Qua2V5cyh0aGlzLl9zb3VyY2VzQ29udGVudHMpLmxlbmd0aCA9PT0gMCkge1xuICAgICAgICB0aGlzLl9zb3VyY2VzQ29udGVudHMgPSBudWxsO1xuICAgICAgfVxuICAgIH1cbiAgfTtcblxuLyoqXG4gKiBBcHBsaWVzIHRoZSBtYXBwaW5ncyBvZiBhIHN1Yi1zb3VyY2UtbWFwIGZvciBhIHNwZWNpZmljIHNvdXJjZSBmaWxlIHRvIHRoZVxuICogc291cmNlIG1hcCBiZWluZyBnZW5lcmF0ZWQuIEVhY2ggbWFwcGluZyB0byB0aGUgc3VwcGxpZWQgc291cmNlIGZpbGUgaXNcbiAqIHJld3JpdHRlbiB1c2luZyB0aGUgc3VwcGxpZWQgc291cmNlIG1hcC4gTm90ZTogVGhlIHJlc29sdXRpb24gZm9yIHRoZVxuICogcmVzdWx0aW5nIG1hcHBpbmdzIGlzIHRoZSBtaW5pbWl1bSBvZiB0aGlzIG1hcCBhbmQgdGhlIHN1cHBsaWVkIG1hcC5cbiAqXG4gKiBAcGFyYW0gYVNvdXJjZU1hcENvbnN1bWVyIFRoZSBzb3VyY2UgbWFwIHRvIGJlIGFwcGxpZWQuXG4gKiBAcGFyYW0gYVNvdXJjZUZpbGUgT3B0aW9uYWwuIFRoZSBmaWxlbmFtZSBvZiB0aGUgc291cmNlIGZpbGUuXG4gKiAgICAgICAgSWYgb21pdHRlZCwgU291cmNlTWFwQ29uc3VtZXIncyBmaWxlIHByb3BlcnR5IHdpbGwgYmUgdXNlZC5cbiAqIEBwYXJhbSBhU291cmNlTWFwUGF0aCBPcHRpb25hbC4gVGhlIGRpcm5hbWUgb2YgdGhlIHBhdGggdG8gdGhlIHNvdXJjZSBtYXBcbiAqICAgICAgICB0byBiZSBhcHBsaWVkLiBJZiByZWxhdGl2ZSwgaXQgaXMgcmVsYXRpdmUgdG8gdGhlIFNvdXJjZU1hcENvbnN1bWVyLlxuICogICAgICAgIFRoaXMgcGFyYW1ldGVyIGlzIG5lZWRlZCB3aGVuIHRoZSB0d28gc291cmNlIG1hcHMgYXJlbid0IGluIHRoZSBzYW1lXG4gKiAgICAgICAgZGlyZWN0b3J5LCBhbmQgdGhlIHNvdXJjZSBtYXAgdG8gYmUgYXBwbGllZCBjb250YWlucyByZWxhdGl2ZSBzb3VyY2VcbiAqICAgICAgICBwYXRocy4gSWYgc28sIHRob3NlIHJlbGF0aXZlIHNvdXJjZSBwYXRocyBuZWVkIHRvIGJlIHJld3JpdHRlblxuICogICAgICAgIHJlbGF0aXZlIHRvIHRoZSBTb3VyY2VNYXBHZW5lcmF0b3IuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYXBwbHlTb3VyY2VNYXAgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfYXBwbHlTb3VyY2VNYXAoYVNvdXJjZU1hcENvbnN1bWVyLCBhU291cmNlRmlsZSwgYVNvdXJjZU1hcFBhdGgpIHtcbiAgICB2YXIgc291cmNlRmlsZSA9IGFTb3VyY2VGaWxlO1xuICAgIC8vIElmIGFTb3VyY2VGaWxlIGlzIG9taXR0ZWQsIHdlIHdpbGwgdXNlIHRoZSBmaWxlIHByb3BlcnR5IG9mIHRoZSBTb3VyY2VNYXBcbiAgICBpZiAoYVNvdXJjZUZpbGUgPT0gbnVsbCkge1xuICAgICAgaWYgKGFTb3VyY2VNYXBDb25zdW1lci5maWxlID09IG51bGwpIHtcbiAgICAgICAgdGhyb3cgbmV3IEVycm9yKFxuICAgICAgICAgICdTb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLmFwcGx5U291cmNlTWFwIHJlcXVpcmVzIGVpdGhlciBhbiBleHBsaWNpdCBzb3VyY2UgZmlsZSwgJyArXG4gICAgICAgICAgJ29yIHRoZSBzb3VyY2UgbWFwXFwncyBcImZpbGVcIiBwcm9wZXJ0eS4gQm90aCB3ZXJlIG9taXR0ZWQuJ1xuICAgICAgICApO1xuICAgICAgfVxuICAgICAgc291cmNlRmlsZSA9IGFTb3VyY2VNYXBDb25zdW1lci5maWxlO1xuICAgIH1cbiAgICB2YXIgc291cmNlUm9vdCA9IHRoaXMuX3NvdXJjZVJvb3Q7XG4gICAgLy8gTWFrZSBcInNvdXJjZUZpbGVcIiByZWxhdGl2ZSBpZiBhbiBhYnNvbHV0ZSBVcmwgaXMgcGFzc2VkLlxuICAgIGlmIChzb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgIHNvdXJjZUZpbGUgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIHNvdXJjZUZpbGUpO1xuICAgIH1cbiAgICAvLyBBcHBseWluZyB0aGUgU291cmNlTWFwIGNhbiBhZGQgYW5kIHJlbW92ZSBpdGVtcyBmcm9tIHRoZSBzb3VyY2VzIGFuZFxuICAgIC8vIHRoZSBuYW1lcyBhcnJheS5cbiAgICB2YXIgbmV3U291cmNlcyA9IG5ldyBBcnJheVNldCgpO1xuICAgIHZhciBuZXdOYW1lcyA9IG5ldyBBcnJheVNldCgpO1xuXG4gICAgLy8gRmluZCBtYXBwaW5ncyBmb3IgdGhlIFwic291cmNlRmlsZVwiXG4gICAgdGhpcy5fbWFwcGluZ3MudW5zb3J0ZWRGb3JFYWNoKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgPT09IHNvdXJjZUZpbGUgJiYgbWFwcGluZy5vcmlnaW5hbExpbmUgIT0gbnVsbCkge1xuICAgICAgICAvLyBDaGVjayBpZiBpdCBjYW4gYmUgbWFwcGVkIGJ5IHRoZSBzb3VyY2UgbWFwLCB0aGVuIHVwZGF0ZSB0aGUgbWFwcGluZy5cbiAgICAgICAgdmFyIG9yaWdpbmFsID0gYVNvdXJjZU1hcENvbnN1bWVyLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgICAgIGxpbmU6IG1hcHBpbmcub3JpZ2luYWxMaW5lLFxuICAgICAgICAgIGNvbHVtbjogbWFwcGluZy5vcmlnaW5hbENvbHVtblxuICAgICAgICB9KTtcbiAgICAgICAgaWYgKG9yaWdpbmFsLnNvdXJjZSAhPSBudWxsKSB7XG4gICAgICAgICAgLy8gQ29weSBtYXBwaW5nXG4gICAgICAgICAgbWFwcGluZy5zb3VyY2UgPSBvcmlnaW5hbC5zb3VyY2U7XG4gICAgICAgICAgaWYgKGFTb3VyY2VNYXBQYXRoICE9IG51bGwpIHtcbiAgICAgICAgICAgIG1hcHBpbmcuc291cmNlID0gdXRpbC5qb2luKGFTb3VyY2VNYXBQYXRoLCBtYXBwaW5nLnNvdXJjZSlcbiAgICAgICAgICB9XG4gICAgICAgICAgaWYgKHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgICAgbWFwcGluZy5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIG1hcHBpbmcuc291cmNlKTtcbiAgICAgICAgICB9XG4gICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgPSBvcmlnaW5hbC5saW5lO1xuICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxDb2x1bW4gPSBvcmlnaW5hbC5jb2x1bW47XG4gICAgICAgICAgaWYgKG9yaWdpbmFsLm5hbWUgIT0gbnVsbCkge1xuICAgICAgICAgICAgbWFwcGluZy5uYW1lID0gb3JpZ2luYWwubmFtZTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH1cblxuICAgICAgdmFyIHNvdXJjZSA9IG1hcHBpbmcuc291cmNlO1xuICAgICAgaWYgKHNvdXJjZSAhPSBudWxsICYmICFuZXdTb3VyY2VzLmhhcyhzb3VyY2UpKSB7XG4gICAgICAgIG5ld1NvdXJjZXMuYWRkKHNvdXJjZSk7XG4gICAgICB9XG5cbiAgICAgIHZhciBuYW1lID0gbWFwcGluZy5uYW1lO1xuICAgICAgaWYgKG5hbWUgIT0gbnVsbCAmJiAhbmV3TmFtZXMuaGFzKG5hbWUpKSB7XG4gICAgICAgIG5ld05hbWVzLmFkZChuYW1lKTtcbiAgICAgIH1cblxuICAgIH0sIHRoaXMpO1xuICAgIHRoaXMuX3NvdXJjZXMgPSBuZXdTb3VyY2VzO1xuICAgIHRoaXMuX25hbWVzID0gbmV3TmFtZXM7XG5cbiAgICAvLyBDb3B5IHNvdXJjZXNDb250ZW50cyBvZiBhcHBsaWVkIG1hcC5cbiAgICBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlcy5mb3JFYWNoKGZ1bmN0aW9uIChzb3VyY2VGaWxlKSB7XG4gICAgICB2YXIgY29udGVudCA9IGFTb3VyY2VNYXBDb25zdW1lci5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZUZpbGUpO1xuICAgICAgaWYgKGNvbnRlbnQgIT0gbnVsbCkge1xuICAgICAgICBpZiAoYVNvdXJjZU1hcFBhdGggIT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZUZpbGUgPSB1dGlsLmpvaW4oYVNvdXJjZU1hcFBhdGgsIHNvdXJjZUZpbGUpO1xuICAgICAgICB9XG4gICAgICAgIGlmIChzb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgICAgICBzb3VyY2VGaWxlID0gdXRpbC5yZWxhdGl2ZShzb3VyY2VSb290LCBzb3VyY2VGaWxlKTtcbiAgICAgICAgfVxuICAgICAgICB0aGlzLnNldFNvdXJjZUNvbnRlbnQoc291cmNlRmlsZSwgY29udGVudCk7XG4gICAgICB9XG4gICAgfSwgdGhpcyk7XG4gIH07XG5cbi8qKlxuICogQSBtYXBwaW5nIGNhbiBoYXZlIG9uZSBvZiB0aGUgdGhyZWUgbGV2ZWxzIG9mIGRhdGE6XG4gKlxuICogICAxLiBKdXN0IHRoZSBnZW5lcmF0ZWQgcG9zaXRpb24uXG4gKiAgIDIuIFRoZSBHZW5lcmF0ZWQgcG9zaXRpb24sIG9yaWdpbmFsIHBvc2l0aW9uLCBhbmQgb3JpZ2luYWwgc291cmNlLlxuICogICAzLiBHZW5lcmF0ZWQgYW5kIG9yaWdpbmFsIHBvc2l0aW9uLCBvcmlnaW5hbCBzb3VyY2UsIGFzIHdlbGwgYXMgYSBuYW1lXG4gKiAgICAgIHRva2VuLlxuICpcbiAqIFRvIG1haW50YWluIGNvbnNpc3RlbmN5LCB3ZSB2YWxpZGF0ZSB0aGF0IGFueSBuZXcgbWFwcGluZyBiZWluZyBhZGRlZCBmYWxsc1xuICogaW4gdG8gb25lIG9mIHRoZXNlIGNhdGVnb3JpZXMuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuX3ZhbGlkYXRlTWFwcGluZyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl92YWxpZGF0ZU1hcHBpbmcoYUdlbmVyYXRlZCwgYU9yaWdpbmFsLCBhU291cmNlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGFOYW1lKSB7XG4gICAgLy8gV2hlbiBhT3JpZ2luYWwgaXMgdHJ1dGh5IGJ1dCBoYXMgZW1wdHkgdmFsdWVzIGZvciAubGluZSBhbmQgLmNvbHVtbixcbiAgICAvLyBpdCBpcyBtb3N0IGxpa2VseSBhIHByb2dyYW1tZXIgZXJyb3IuIEluIHRoaXMgY2FzZSB3ZSB0aHJvdyBhIHZlcnlcbiAgICAvLyBzcGVjaWZpYyBlcnJvciBtZXNzYWdlIHRvIHRyeSB0byBndWlkZSB0aGVtIHRoZSByaWdodCB3YXkuXG4gICAgLy8gRm9yIGV4YW1wbGU6IGh0dHBzOi8vZ2l0aHViLmNvbS9Qb2x5bWVyL3BvbHltZXItYnVuZGxlci9wdWxsLzUxOVxuICAgIGlmIChhT3JpZ2luYWwgJiYgdHlwZW9mIGFPcmlnaW5hbC5saW5lICE9PSAnbnVtYmVyJyAmJiB0eXBlb2YgYU9yaWdpbmFsLmNvbHVtbiAhPT0gJ251bWJlcicpIHtcbiAgICAgICAgdGhyb3cgbmV3IEVycm9yKFxuICAgICAgICAgICAgJ29yaWdpbmFsLmxpbmUgYW5kIG9yaWdpbmFsLmNvbHVtbiBhcmUgbm90IG51bWJlcnMgLS0geW91IHByb2JhYmx5IG1lYW50IHRvIG9taXQgJyArXG4gICAgICAgICAgICAndGhlIG9yaWdpbmFsIG1hcHBpbmcgZW50aXJlbHkgYW5kIG9ubHkgbWFwIHRoZSBnZW5lcmF0ZWQgcG9zaXRpb24uIElmIHNvLCBwYXNzICcgK1xuICAgICAgICAgICAgJ251bGwgZm9yIHRoZSBvcmlnaW5hbCBtYXBwaW5nIGluc3RlYWQgb2YgYW4gb2JqZWN0IHdpdGggZW1wdHkgb3IgbnVsbCB2YWx1ZXMuJ1xuICAgICAgICApO1xuICAgIH1cblxuICAgIGlmIChhR2VuZXJhdGVkICYmICdsaW5lJyBpbiBhR2VuZXJhdGVkICYmICdjb2x1bW4nIGluIGFHZW5lcmF0ZWRcbiAgICAgICAgJiYgYUdlbmVyYXRlZC5saW5lID4gMCAmJiBhR2VuZXJhdGVkLmNvbHVtbiA+PSAwXG4gICAgICAgICYmICFhT3JpZ2luYWwgJiYgIWFTb3VyY2UgJiYgIWFOYW1lKSB7XG4gICAgICAvLyBDYXNlIDEuXG4gICAgICByZXR1cm47XG4gICAgfVxuICAgIGVsc2UgaWYgKGFHZW5lcmF0ZWQgJiYgJ2xpbmUnIGluIGFHZW5lcmF0ZWQgJiYgJ2NvbHVtbicgaW4gYUdlbmVyYXRlZFxuICAgICAgICAgICAgICYmIGFPcmlnaW5hbCAmJiAnbGluZScgaW4gYU9yaWdpbmFsICYmICdjb2x1bW4nIGluIGFPcmlnaW5hbFxuICAgICAgICAgICAgICYmIGFHZW5lcmF0ZWQubGluZSA+IDAgJiYgYUdlbmVyYXRlZC5jb2x1bW4gPj0gMFxuICAgICAgICAgICAgICYmIGFPcmlnaW5hbC5saW5lID4gMCAmJiBhT3JpZ2luYWwuY29sdW1uID49IDBcbiAgICAgICAgICAgICAmJiBhU291cmNlKSB7XG4gICAgICAvLyBDYXNlcyAyIGFuZCAzLlxuICAgICAgcmV0dXJuO1xuICAgIH1cbiAgICBlbHNlIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcignSW52YWxpZCBtYXBwaW5nOiAnICsgSlNPTi5zdHJpbmdpZnkoe1xuICAgICAgICBnZW5lcmF0ZWQ6IGFHZW5lcmF0ZWQsXG4gICAgICAgIHNvdXJjZTogYVNvdXJjZSxcbiAgICAgICAgb3JpZ2luYWw6IGFPcmlnaW5hbCxcbiAgICAgICAgbmFtZTogYU5hbWVcbiAgICAgIH0pKTtcbiAgICB9XG4gIH07XG5cbi8qKlxuICogU2VyaWFsaXplIHRoZSBhY2N1bXVsYXRlZCBtYXBwaW5ncyBpbiB0byB0aGUgc3RyZWFtIG9mIGJhc2UgNjQgVkxRc1xuICogc3BlY2lmaWVkIGJ5IHRoZSBzb3VyY2UgbWFwIGZvcm1hdC5cbiAqL1xuU291cmNlTWFwR2VuZXJhdG9yLnByb3RvdHlwZS5fc2VyaWFsaXplTWFwcGluZ3MgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3Jfc2VyaWFsaXplTWFwcGluZ3MoKSB7XG4gICAgdmFyIHByZXZpb3VzR2VuZXJhdGVkQ29sdW1uID0gMDtcbiAgICB2YXIgcHJldmlvdXNHZW5lcmF0ZWRMaW5lID0gMTtcbiAgICB2YXIgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IDA7XG4gICAgdmFyIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gMDtcbiAgICB2YXIgcHJldmlvdXNOYW1lID0gMDtcbiAgICB2YXIgcHJldmlvdXNTb3VyY2UgPSAwO1xuICAgIHZhciByZXN1bHQgPSAnJztcbiAgICB2YXIgbmV4dDtcbiAgICB2YXIgbWFwcGluZztcbiAgICB2YXIgbmFtZUlkeDtcbiAgICB2YXIgc291cmNlSWR4O1xuXG4gICAgdmFyIG1hcHBpbmdzID0gdGhpcy5fbWFwcGluZ3MudG9BcnJheSgpO1xuICAgIGZvciAodmFyIGkgPSAwLCBsZW4gPSBtYXBwaW5ncy5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgICAgbWFwcGluZyA9IG1hcHBpbmdzW2ldO1xuICAgICAgbmV4dCA9ICcnXG5cbiAgICAgIGlmIChtYXBwaW5nLmdlbmVyYXRlZExpbmUgIT09IHByZXZpb3VzR2VuZXJhdGVkTGluZSkge1xuICAgICAgICBwcmV2aW91c0dlbmVyYXRlZENvbHVtbiA9IDA7XG4gICAgICAgIHdoaWxlIChtYXBwaW5nLmdlbmVyYXRlZExpbmUgIT09IHByZXZpb3VzR2VuZXJhdGVkTGluZSkge1xuICAgICAgICAgIG5leHQgKz0gJzsnO1xuICAgICAgICAgIHByZXZpb3VzR2VuZXJhdGVkTGluZSsrO1xuICAgICAgICB9XG4gICAgICB9XG4gICAgICBlbHNlIHtcbiAgICAgICAgaWYgKGkgPiAwKSB7XG4gICAgICAgICAgaWYgKCF1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmcsIG1hcHBpbmdzW2kgLSAxXSkpIHtcbiAgICAgICAgICAgIGNvbnRpbnVlO1xuICAgICAgICAgIH1cbiAgICAgICAgICBuZXh0ICs9ICcsJztcbiAgICAgICAgfVxuICAgICAgfVxuXG4gICAgICBuZXh0ICs9IGJhc2U2NFZMUS5lbmNvZGUobWFwcGluZy5nZW5lcmF0ZWRDb2x1bW5cbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIC0gcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4pO1xuICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgaWYgKG1hcHBpbmcuc291cmNlICE9IG51bGwpIHtcbiAgICAgICAgc291cmNlSWR4ID0gdGhpcy5fc291cmNlcy5pbmRleE9mKG1hcHBpbmcuc291cmNlKTtcbiAgICAgICAgbmV4dCArPSBiYXNlNjRWTFEuZW5jb2RlKHNvdXJjZUlkeCAtIHByZXZpb3VzU291cmNlKTtcbiAgICAgICAgcHJldmlvdXNTb3VyY2UgPSBzb3VyY2VJZHg7XG5cbiAgICAgICAgLy8gbGluZXMgYXJlIHN0b3JlZCAwLWJhc2VkIGluIFNvdXJjZU1hcCBzcGVjIHZlcnNpb24gM1xuICAgICAgICBuZXh0ICs9IGJhc2U2NFZMUS5lbmNvZGUobWFwcGluZy5vcmlnaW5hbExpbmUgLSAxXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIC0gcHJldmlvdXNPcmlnaW5hbExpbmUpO1xuICAgICAgICBwcmV2aW91c09yaWdpbmFsTGluZSA9IG1hcHBpbmcub3JpZ2luYWxMaW5lIC0gMTtcblxuICAgICAgICBuZXh0ICs9IGJhc2U2NFZMUS5lbmNvZGUobWFwcGluZy5vcmlnaW5hbENvbHVtblxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAtIHByZXZpb3VzT3JpZ2luYWxDb2x1bW4pO1xuICAgICAgICBwcmV2aW91c09yaWdpbmFsQ29sdW1uID0gbWFwcGluZy5vcmlnaW5hbENvbHVtbjtcblxuICAgICAgICBpZiAobWFwcGluZy5uYW1lICE9IG51bGwpIHtcbiAgICAgICAgICBuYW1lSWR4ID0gdGhpcy5fbmFtZXMuaW5kZXhPZihtYXBwaW5nLm5hbWUpO1xuICAgICAgICAgIG5leHQgKz0gYmFzZTY0VkxRLmVuY29kZShuYW1lSWR4IC0gcHJldmlvdXNOYW1lKTtcbiAgICAgICAgICBwcmV2aW91c05hbWUgPSBuYW1lSWR4O1xuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIHJlc3VsdCArPSBuZXh0O1xuICAgIH1cblxuICAgIHJldHVybiByZXN1bHQ7XG4gIH07XG5cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuX2dlbmVyYXRlU291cmNlc0NvbnRlbnQgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfZ2VuZXJhdGVTb3VyY2VzQ29udGVudChhU291cmNlcywgYVNvdXJjZVJvb3QpIHtcbiAgICByZXR1cm4gYVNvdXJjZXMubWFwKGZ1bmN0aW9uIChzb3VyY2UpIHtcbiAgICAgIGlmICghdGhpcy5fc291cmNlc0NvbnRlbnRzKSB7XG4gICAgICAgIHJldHVybiBudWxsO1xuICAgICAgfVxuICAgICAgaWYgKGFTb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgICAgc291cmNlID0gdXRpbC5yZWxhdGl2ZShhU291cmNlUm9vdCwgc291cmNlKTtcbiAgICAgIH1cbiAgICAgIHZhciBrZXkgPSB1dGlsLnRvU2V0U3RyaW5nKHNvdXJjZSk7XG4gICAgICByZXR1cm4gT2JqZWN0LnByb3RvdHlwZS5oYXNPd25Qcm9wZXJ0eS5jYWxsKHRoaXMuX3NvdXJjZXNDb250ZW50cywga2V5KVxuICAgICAgICA/IHRoaXMuX3NvdXJjZXNDb250ZW50c1trZXldXG4gICAgICAgIDogbnVsbDtcbiAgICB9LCB0aGlzKTtcbiAgfTtcblxuLyoqXG4gKiBFeHRlcm5hbGl6ZSB0aGUgc291cmNlIG1hcC5cbiAqL1xuU291cmNlTWFwR2VuZXJhdG9yLnByb3RvdHlwZS50b0pTT04gPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfdG9KU09OKCkge1xuICAgIHZhciBtYXAgPSB7XG4gICAgICB2ZXJzaW9uOiB0aGlzLl92ZXJzaW9uLFxuICAgICAgc291cmNlczogdGhpcy5fc291cmNlcy50b0FycmF5KCksXG4gICAgICBuYW1lczogdGhpcy5fbmFtZXMudG9BcnJheSgpLFxuICAgICAgbWFwcGluZ3M6IHRoaXMuX3NlcmlhbGl6ZU1hcHBpbmdzKClcbiAgICB9O1xuICAgIGlmICh0aGlzLl9maWxlICE9IG51bGwpIHtcbiAgICAgIG1hcC5maWxlID0gdGhpcy5fZmlsZTtcbiAgICB9XG4gICAgaWYgKHRoaXMuX3NvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgbWFwLnNvdXJjZVJvb3QgPSB0aGlzLl9zb3VyY2VSb290O1xuICAgIH1cbiAgICBpZiAodGhpcy5fc291cmNlc0NvbnRlbnRzKSB7XG4gICAgICBtYXAuc291cmNlc0NvbnRlbnQgPSB0aGlzLl9nZW5lcmF0ZVNvdXJjZXNDb250ZW50KG1hcC5zb3VyY2VzLCBtYXAuc291cmNlUm9vdCk7XG4gICAgfVxuXG4gICAgcmV0dXJuIG1hcDtcbiAgfTtcblxuLyoqXG4gKiBSZW5kZXIgdGhlIHNvdXJjZSBtYXAgYmVpbmcgZ2VuZXJhdGVkIHRvIGEgc3RyaW5nLlxuICovXG5Tb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLnRvU3RyaW5nID1cbiAgZnVuY3Rpb24gU291cmNlTWFwR2VuZXJhdG9yX3RvU3RyaW5nKCkge1xuICAgIHJldHVybiBKU09OLnN0cmluZ2lmeSh0aGlzLnRvSlNPTigpKTtcbiAgfTtcblxuZXhwb3J0cy5Tb3VyY2VNYXBHZW5lcmF0b3IgPSBTb3VyY2VNYXBHZW5lcmF0b3I7XG5cblxuXG4vLy8vLy8vLy8vLy8vLy8vLy9cbi8vIFdFQlBBQ0sgRk9PVEVSXG4vLyAuL2xpYi9zb3VyY2UtbWFwLWdlbmVyYXRvci5qc1xuLy8gbW9kdWxlIGlkID0gOVxuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTQgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbnZhciB1dGlsID0gcmVxdWlyZSgnLi91dGlsJyk7XG5cbi8qKlxuICogRGV0ZXJtaW5lIHdoZXRoZXIgbWFwcGluZ0IgaXMgYWZ0ZXIgbWFwcGluZ0Egd2l0aCByZXNwZWN0IHRvIGdlbmVyYXRlZFxuICogcG9zaXRpb24uXG4gKi9cbmZ1bmN0aW9uIGdlbmVyYXRlZFBvc2l0aW9uQWZ0ZXIobWFwcGluZ0EsIG1hcHBpbmdCKSB7XG4gIC8vIE9wdGltaXplZCBmb3IgbW9zdCBjb21tb24gY2FzZVxuICB2YXIgbGluZUEgPSBtYXBwaW5nQS5nZW5lcmF0ZWRMaW5lO1xuICB2YXIgbGluZUIgPSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICB2YXIgY29sdW1uQSA9IG1hcHBpbmdBLmdlbmVyYXRlZENvbHVtbjtcbiAgdmFyIGNvbHVtbkIgPSBtYXBwaW5nQi5nZW5lcmF0ZWRDb2x1bW47XG4gIHJldHVybiBsaW5lQiA+IGxpbmVBIHx8IGxpbmVCID09IGxpbmVBICYmIGNvbHVtbkIgPj0gY29sdW1uQSB8fFxuICAgICAgICAgdXRpbC5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZChtYXBwaW5nQSwgbWFwcGluZ0IpIDw9IDA7XG59XG5cbi8qKlxuICogQSBkYXRhIHN0cnVjdHVyZSB0byBwcm92aWRlIGEgc29ydGVkIHZpZXcgb2YgYWNjdW11bGF0ZWQgbWFwcGluZ3MgaW4gYVxuICogcGVyZm9ybWFuY2UgY29uc2Npb3VzIG1hbm5lci4gSXQgdHJhZGVzIGEgbmVnbGliYWJsZSBvdmVyaGVhZCBpbiBnZW5lcmFsXG4gKiBjYXNlIGZvciBhIGxhcmdlIHNwZWVkdXAgaW4gY2FzZSBvZiBtYXBwaW5ncyBiZWluZyBhZGRlZCBpbiBvcmRlci5cbiAqL1xuZnVuY3Rpb24gTWFwcGluZ0xpc3QoKSB7XG4gIHRoaXMuX2FycmF5ID0gW107XG4gIHRoaXMuX3NvcnRlZCA9IHRydWU7XG4gIC8vIFNlcnZlcyBhcyBpbmZpbXVtXG4gIHRoaXMuX2xhc3QgPSB7Z2VuZXJhdGVkTGluZTogLTEsIGdlbmVyYXRlZENvbHVtbjogMH07XG59XG5cbi8qKlxuICogSXRlcmF0ZSB0aHJvdWdoIGludGVybmFsIGl0ZW1zLiBUaGlzIG1ldGhvZCB0YWtlcyB0aGUgc2FtZSBhcmd1bWVudHMgdGhhdFxuICogYEFycmF5LnByb3RvdHlwZS5mb3JFYWNoYCB0YWtlcy5cbiAqXG4gKiBOT1RFOiBUaGUgb3JkZXIgb2YgdGhlIG1hcHBpbmdzIGlzIE5PVCBndWFyYW50ZWVkLlxuICovXG5NYXBwaW5nTGlzdC5wcm90b3R5cGUudW5zb3J0ZWRGb3JFYWNoID1cbiAgZnVuY3Rpb24gTWFwcGluZ0xpc3RfZm9yRWFjaChhQ2FsbGJhY2ssIGFUaGlzQXJnKSB7XG4gICAgdGhpcy5fYXJyYXkuZm9yRWFjaChhQ2FsbGJhY2ssIGFUaGlzQXJnKTtcbiAgfTtcblxuLyoqXG4gKiBBZGQgdGhlIGdpdmVuIHNvdXJjZSBtYXBwaW5nLlxuICpcbiAqIEBwYXJhbSBPYmplY3QgYU1hcHBpbmdcbiAqL1xuTWFwcGluZ0xpc3QucHJvdG90eXBlLmFkZCA9IGZ1bmN0aW9uIE1hcHBpbmdMaXN0X2FkZChhTWFwcGluZykge1xuICBpZiAoZ2VuZXJhdGVkUG9zaXRpb25BZnRlcih0aGlzLl9sYXN0LCBhTWFwcGluZykpIHtcbiAgICB0aGlzLl9sYXN0ID0gYU1hcHBpbmc7XG4gICAgdGhpcy5fYXJyYXkucHVzaChhTWFwcGluZyk7XG4gIH0gZWxzZSB7XG4gICAgdGhpcy5fc29ydGVkID0gZmFsc2U7XG4gICAgdGhpcy5fYXJyYXkucHVzaChhTWFwcGluZyk7XG4gIH1cbn07XG5cbi8qKlxuICogUmV0dXJucyB0aGUgZmxhdCwgc29ydGVkIGFycmF5IG9mIG1hcHBpbmdzLiBUaGUgbWFwcGluZ3MgYXJlIHNvcnRlZCBieVxuICogZ2VuZXJhdGVkIHBvc2l0aW9uLlxuICpcbiAqIFdBUk5JTkc6IFRoaXMgbWV0aG9kIHJldHVybnMgaW50ZXJuYWwgZGF0YSB3aXRob3V0IGNvcHlpbmcsIGZvclxuICogcGVyZm9ybWFuY2UuIFRoZSByZXR1cm4gdmFsdWUgbXVzdCBOT1QgYmUgbXV0YXRlZCwgYW5kIHNob3VsZCBiZSB0cmVhdGVkIGFzXG4gKiBhbiBpbW11dGFibGUgYm9ycm93LiBJZiB5b3Ugd2FudCB0byB0YWtlIG93bmVyc2hpcCwgeW91IG11c3QgbWFrZSB5b3VyIG93blxuICogY29weS5cbiAqL1xuTWFwcGluZ0xpc3QucHJvdG90eXBlLnRvQXJyYXkgPSBmdW5jdGlvbiBNYXBwaW5nTGlzdF90b0FycmF5KCkge1xuICBpZiAoIXRoaXMuX3NvcnRlZCkge1xuICAgIHRoaXMuX2FycmF5LnNvcnQodXRpbC5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZCk7XG4gICAgdGhpcy5fc29ydGVkID0gdHJ1ZTtcbiAgfVxuICByZXR1cm4gdGhpcy5fYXJyYXk7XG59O1xuXG5leHBvcnRzLk1hcHBpbmdMaXN0ID0gTWFwcGluZ0xpc3Q7XG5cblxuXG4vLy8vLy8vLy8vLy8vLy8vLy9cbi8vIFdFQlBBQ0sgRk9PVEVSXG4vLyAuL2xpYi9tYXBwaW5nLWxpc3QuanNcbi8vIG1vZHVsZSBpZCA9IDEwXG4vLyBtb2R1bGUgY2h1bmtzID0gMCJdLCJzb3VyY2VSb290IjoiIn0=