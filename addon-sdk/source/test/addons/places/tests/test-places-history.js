/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cc, Ci } = require('chrome');
const { defer, all } = require('sdk/core/promise');
const { has } = require('sdk/util/array');
const { setTimeout } = require('sdk/timers');
const { before, after } = require('sdk/test/utils');
const { set } = require('sdk/preferences/service');
const {
  search 
} = require('sdk/places/history');
const {
  invalidResolve, invalidReject, createTree,
  compareWithHost, addVisits, resetPlaces
} = require('../places-helper');
const { promisedEmitter } = require('sdk/places/utils');

exports.testEmptyQuery = function (assert, done) {
  let within = toBeWithin();
  addVisits([
    'http://simplequery-1.com', 'http://simplequery-2.com'
  ]).then(searchP).then(results => {
    assert.equal(results.length, 2, 'Correct number of entries returned');
    assert.equal(results[0].url, 'http://simplequery-1.com/',
      'matches url');
    assert.equal(results[1].url, 'http://simplequery-2.com/',
      'matches url');
    assert.equal(results[0].title, 'Test visit for ' + results[0].url,
      'title matches');
    assert.equal(results[1].title, 'Test visit for ' + results[1].url,
      'title matches');
    assert.equal(results[0].visitCount, 1, 'matches access');
    assert.equal(results[1].visitCount, 1, 'matches access');
    assert.ok(within(results[0].time), 'accurate access time');
    assert.ok(within(results[1].time), 'accurate access time');
    assert.equal(Object.keys(results[0]).length, 4,
      'no addition exposed properties on history result');
    done();
  }, invalidReject);
};

exports.testVisitCount = function (assert, done) {
  addVisits([
    'http://simplequery-1.com', 'http://simplequery-1.com',
    'http://simplequery-1.com', 'http://simplequery-1.com'
  ]).then(searchP).then(results => {
    assert.equal(results.length, 1, 'Correct number of entries returned');
    assert.equal(results[0].url, 'http://simplequery-1.com/', 'correct url');
    assert.equal(results[0].visitCount, 4, 'matches access count');
    done();
  }, invalidReject);
};

/*
 * Tests 4 scenarios
 * '*.mozilla.org'
 * 'mozilla.org'
 * 'http://mozilla.org/'
 * 'http://mozilla.org/*'
 */
exports.testSearchURL = function (assert, done) {
  addVisits([
    'http://developer.mozilla.org', 'http://mozilla.org',
    'http://mozilla.org/index', 'https://mozilla.org'
  ]).then(() => searchP({ url: '*.mozilla.org' }))
  .then(results => {
    assert.equal(results.length, 4, 'returns all entries');
    return searchP({ url: 'mozilla.org' });
  }).then(results => {
    assert.equal(results.length, 3, 'returns entries where mozilla.org is host');
    return searchP({ url: 'http://mozilla.org/' });
  }).then(results => {
    assert.equal(results.length, 1, 'should just be an exact match');
    return searchP({ url: 'http://mozilla.org/*' });
  }).then(results => {
    assert.equal(results.length, 2, 'should match anything starting with substring');
    done();
  });
};

// Disabling due to intermittent Bug 892619
// TODO solve this
/*
exports.testSearchTimeRange = function (assert, done) {
  let firstTime, secondTime;
  addVisits([
    'http://earlyvisit.org', 'http://earlyvisit.org/earlytown.html'
  ]).then(searchP).then(results => {
    firstTime = results[0].time;
    var deferred = defer();
    setTimeout(function () deferred.resolve(), 1000);
    return deferred.promise;
  }).then(() => {
    return addVisits(['http://newvisit.org', 'http://newvisit.org/whoawhoa.html']);
  }).then(searchP).then(results => {
    results.filter(({url, time}) => {
      if (/newvisit/.test(url)) secondTime = time;
    });
    return searchP({ from: firstTime - 1000 });
  }).then(results => {
    assert.equal(results.length, 4, 'should return all entries');
    return searchP({ to: firstTime + 500 });
  }).then(results => {
    assert.equal(results.length, 2, 'should return only first entries');
    results.map(item => {
      assert.ok(/earlyvisit/.test(item.url), 'correct entry');
    });
    return searchP({ from: firstTime + 500 });
  }).then(results => {
    assert.equal(results.length, 2, 'should return only last entries');
    results.map(item => {
      assert.ok(/newvisit/.test(item.url), 'correct entry');
    });
    done();
  });
};
*/
exports.testSearchQuery = function (assert, done) {
  addVisits([
    'http://mozilla.com', 'http://webaud.io', 'http://mozilla.com/webfwd'
  ]).then(() => {
    return searchP({ query: 'moz' });
  }).then(results => {
    assert.equal(results.length, 2, 'should return urls that match substring');
    results.map(({url}) => {
      assert.ok(/moz/.test(url), 'correct item');
    });
    return searchP([{ query: 'webfwd' }, { query: 'aud.io' }]);
  }).then(results => {
    assert.equal(results.length, 2, 'should OR separate queries');
    results.map(({url}) => {
      assert.ok(/webfwd|aud\.io/.test(url), 'correct item');
    });
    done();
  });
};

/*
 * Query Options
 */

exports.testSearchCount = function (assert, done) {
  addVisits([
    'http://mozilla.com', 'http://webaud.io', 'http://mozilla.com/webfwd',
    'http://developer.mozilla.com', 'http://bandcamp.com'
  ]).then(testCount(1))
  .then(testCount(2))
  .then(testCount(3))
  .then(testCount(5))
  .then(done);

  function testCount (n) {
    return function () {
      return searchP({}, { count: n }).then(results => {
        assert.equal(results.length, n,
          'count ' + n + ' returns ' + n + ' results');
      });
    };
  }
};

exports.testSearchSort = function (assert, done) {
  let places = [
    'http://mozilla.com/', 'http://webaud.io/', 'http://mozilla.com/webfwd/',
    'http://developer.mozilla.com/', 'http://bandcamp.com/'
  ];
  addVisits(places).then(() => {
    return searchP({}, { sort: 'title' });
  }).then(results => {
    checkOrder(results, [4,3,0,2,1]);
    return searchP({}, { sort: 'title', descending: true });
  }).then(results => {
    checkOrder(results, [1,2,0,3,4]);
    return searchP({}, { sort: 'url' });
  }).then(results => {
    checkOrder(results, [4,3,0,2,1]);
    return searchP({}, { sort: 'url', descending: true });
  }).then(results => {
    checkOrder(results, [1,2,0,3,4]);
    return addVisits('http://mozilla.com') // for visit conut
      .then(() => addVisits('http://github.com')); // for checking date
  }).then(() => {
    return searchP({}, { sort: 'visitCount' });
  }).then(results => {
    assert.equal(results[5].url, 'http://mozilla.com/',
      'last entry is the highest visit count');
    return searchP({}, { sort: 'visitCount', descending: true });
  }).then(results => {
    assert.equal(results[0].url, 'http://mozilla.com/',
      'first entry is the highest visit count');
    return searchP({}, { sort: 'date' });
  }).then(results => {
    assert.equal(results[5].url, 'http://github.com/',
      'latest visited should be first');
    return searchP({}, { sort: 'date', descending: true });
  }).then(results => {
    assert.equal(results[0].url, 'http://github.com/',
      'latest visited should be at the end');
  }).then(done);

  function checkOrder (results, nums) {
    assert.equal(results.length, nums.length, 'expected return count');
    for (let i = 0; i < nums.length; i++) {
      assert.equal(results[i].url, places[nums[i]], 'successful order');
    }
  }
};

exports.testEmitters = function (assert, done) {
  let urls = [
    'http://mozilla.com/', 'http://webaud.io/', 'http://mozilla.com/webfwd/',
    'http://developer.mozilla.com/', 'http://bandcamp.com/'
  ];
  addVisits(urls).then(() => {
    let count = 0;
    search().on('data', item => {
      assert.ok(~urls.indexOf(item.url), 'data value found in url list');
      count++;
    }).on('end', results => {
      assert.equal(results.length, 5, 'correct count of items');
      assert.equal(count, 5, 'data event called 5 times');
      done();
    });
  });
};

function toBeWithin (range) {
  range = range || 2000;
  var current = new Date() * 1000; // convert to microseconds
  return compared => { 
    return compared - current < range;
  };
}

function searchP () {
  return promisedEmitter(search.apply(null, Array.slice(arguments)));
}

before(exports, (name, assert, done) => resetPlaces(done));
after(exports, (name, assert, done) => resetPlaces(done));
