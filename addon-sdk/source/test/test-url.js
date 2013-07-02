/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { URL, toFilename, fromFilename, isValidURI, getTLD, DataURL } = require('sdk/url');
const { pathFor } = require('sdk/system');
const file = require('sdk/io/file');
const tabs = require('sdk/tabs');
const { decode } = require('sdk/base64');

const httpd = require('sdk/test/httpd');
const port = 8099;

const defaultLocation = '{\'scheme\':\'about\',\'userPass\':null,\'host\':null,\'hostname\':null,\'port\':null,\'path\':\'addons\',\'pathname\':\'addons\',\'hash\':\'\',\'href\':\'about:addons\',\'origin\':\'about:\',\'protocol\':\'about:\',\'search\':\'\'}'.replace(/'/g, '"');

exports.testResolve = function(assert) {
  assert.equal(URL('bar', 'http://www.foo.com/').toString(),
                   'http://www.foo.com/bar');

  assert.equal(URL('bar', 'http://www.foo.com'),
                   'http://www.foo.com/bar');

  assert.equal(URL('http://bar.com/', 'http://foo.com/'),
                   'http://bar.com/',
                   'relative should override base');

  assert.throws(function() { URL('blah'); },
                    /malformed URI: blah/i,
                    'url.resolve() should throw malformed URI on base');

  assert.throws(function() { URL('chrome://global'); },
                    /invalid URI: chrome:\/\/global/i,
                    'url.resolve() should throw invalid URI on base');

  assert.throws(function() { URL('chrome://foo/bar'); },
                    /invalid URI: chrome:\/\/foo\/bar/i,
                    'url.resolve() should throw on bad chrome URI');

  assert.equal(URL('', 'http://www.foo.com'),
                   'http://www.foo.com/',
                   'url.resolve() should add slash to end of domain');
};

exports.testParseHttp = function(assert) {
  var aUrl = 'http://sub.foo.com/bar?locale=en-US&otherArg=%20x%20#myhash';
  var info = URL(aUrl);

  assert.equal(info.scheme, 'http');
  assert.equal(info.protocol, 'http:');
  assert.equal(info.host, 'sub.foo.com');
  assert.equal(info.hostname, 'sub.foo.com');
  assert.equal(info.port, null);
  assert.equal(info.userPass, null);
  assert.equal(info.path, '/bar?locale=en-US&otherArg=%20x%20#myhash');
  assert.equal(info.pathname, '/bar');
  assert.equal(info.href, aUrl);
  assert.equal(info.hash, '#myhash');
  assert.equal(info.search, '?locale=en-US&otherArg=%20x%20');
};

exports.testParseHttpSearchAndHash = function (assert) {
  var info = URL('https://www.moz.com/some/page.html');
  assert.equal(info.hash, '');
  assert.equal(info.search, '');
  
  var hashOnly = URL('https://www.sub.moz.com/page.html#justhash');
  assert.equal(hashOnly.search, '');
  assert.equal(hashOnly.hash, '#justhash');
  
  var queryOnly = URL('https://www.sub.moz.com/page.html?my=query');
  assert.equal(queryOnly.search, '?my=query');
  assert.equal(queryOnly.hash, '');

  var qMark = URL('http://www.moz.org?');
  assert.equal(qMark.search, '');
  assert.equal(qMark.hash, '');
  
  var hash = URL('http://www.moz.org#');
  assert.equal(hash.search, '');
  assert.equal(hash.hash, '');
  
  var empty = URL('http://www.moz.org?#');
  assert.equal(hash.search, '');
  assert.equal(hash.hash, '');

  var strange = URL('http://moz.org?test1#test2?test3');
  assert.equal(strange.search, '?test1');
  assert.equal(strange.hash, '#test2?test3');
};

exports.testParseHttpWithPort = function(assert) {
  var info = URL('http://foo.com:5/bar');
  assert.equal(info.port, 5);
};

exports.testParseChrome = function(assert) {
  var info = URL('chrome://global/content/blah');
  assert.equal(info.scheme, 'chrome');
  assert.equal(info.host, 'global');
  assert.equal(info.port, null);
  assert.equal(info.userPass, null);
  assert.equal(info.path, '/content/blah');
};

exports.testParseAbout = function(assert) {
  var info = URL('about:boop');
  assert.equal(info.scheme, 'about');
  assert.equal(info.host, null);
  assert.equal(info.port, null);
  assert.equal(info.userPass, null);
  assert.equal(info.path, 'boop');
};

exports.testParseFTP = function(assert) {
  var info = URL('ftp://1.2.3.4/foo');
  assert.equal(info.scheme, 'ftp');
  assert.equal(info.host, '1.2.3.4');
  assert.equal(info.port, null);
  assert.equal(info.userPass, null);
  assert.equal(info.path, '/foo');
};

exports.testParseFTPWithUserPass = function(assert) {
  var info = URL('ftp://user:pass@1.2.3.4/foo');
  assert.equal(info.userPass, 'user:pass');
};

exports.testToFilename = function(assert) {
  assert.throws(
    function() { toFilename('resource://nonexistent'); },
    /resource does not exist: resource:\/\/nonexistent\//i,
    'toFilename() on nonexistent resources should throw'
  );

  assert.throws(
    function() { toFilename('http://foo.com/'); },
    /cannot map to filename: http:\/\/foo.com\//i,
    'toFilename() on http: URIs should raise error'
  );

  try {
    assert.ok(
      /.*console\.xul$/.test(toFilename('chrome://global/content/console.xul')),
      'toFilename() w/ console.xul works when it maps to filesystem'
    );
  }
  catch (e) {
    if (/chrome url isn\'t on filesystem/.test(e.message))
      assert.pass('accessing console.xul in jar raises exception');
    else
      assert.fail('accessing console.xul raises ' + e);
  }

  // TODO: Are there any chrome URLs that we're certain exist on the
  // filesystem?
  // assert.ok(/.*main\.js$/.test(toFilename('chrome://myapp/content/main.js')));
};

exports.testFromFilename = function(assert) {
  var profileDirName = require('sdk/system').pathFor('ProfD');
  var fileUrl = fromFilename(profileDirName);
  assert.equal(URL(fileUrl).scheme, 'file',
                   'toFilename() should return a file: url');
  assert.equal(fromFilename(toFilename(fileUrl)), fileUrl);
};

exports.testURL = function(assert) {
  assert.ok(URL('h:foo') instanceof URL, 'instance is of correct type');
  assert.throws(function() URL(),
                    /malformed URI: undefined/i,
                    'url.URL should throw on undefined');
  assert.throws(function() URL(''),
                    /malformed URI: /i,
                    'url.URL should throw on empty string');
  assert.throws(function() URL('foo'),
                    /malformed URI: foo/i,
                    'url.URL should throw on invalid URI');
  assert.ok(URL('h:foo').scheme, 'has scheme');
  assert.equal(URL('h:foo').toString(),
                   'h:foo',
                   'toString should roundtrip');
  // test relative + base
  assert.equal(URL('mypath', 'http://foo').toString(),
                   'http://foo/mypath',
                   'relative URL resolved to base');
  // test relative + no base
  assert.throws(function() URL('path').toString(),
                    /malformed URI: path/i,
                    'no base for relative URI should throw');

  let a = URL('h:foo');
  let b = URL(a);
  assert.equal(b.toString(),
                   'h:foo',
                   'a URL can be initialized from another URL');
  assert.notStrictEqual(a, b,
                            'a URL initialized from another URL is not the same object');
  assert.ok(a == 'h:foo',
              'toString is implicit when a URL is compared to a string via ==');
  assert.strictEqual(a + '', 'h:foo',
                         'toString is implicit when a URL is concatenated to a string');
};

exports.testStringInterface = function(assert) {
  var EM = 'about:addons';
  var a = URL(EM);

  // make sure the standard URL properties are enumerable and not the String interface bits
  assert.equal(Object.keys(a),
    'scheme,userPass,host,hostname,port,path,pathname,hash,href,origin,protocol,search',
    'enumerable key list check for URL.');
  assert.equal(
      JSON.stringify(a),
      defaultLocation,
      'JSON.stringify should return a object with correct props and vals.');

  // make sure that the String interface exists and works as expected
  assert.equal(a.indexOf(':'), EM.indexOf(':'), 'indexOf on URL works');
  assert.equal(a.valueOf(), EM.valueOf(), 'valueOf on URL works.');
  assert.equal(a.toSource(), EM.toSource(), 'toSource on URL works.');
  assert.equal(a.lastIndexOf('a'), EM.lastIndexOf('a'), 'lastIndexOf on URL works.');
  assert.equal(a.match('t:').toString(), EM.match('t:').toString(), 'match on URL works.');
  assert.equal(a.toUpperCase(), EM.toUpperCase(), 'toUpperCase on URL works.');
  assert.equal(a.toLowerCase(), EM.toLowerCase(), 'toLowerCase on URL works.');
  assert.equal(a.split(':').toString(), EM.split(':').toString(), 'split on URL works.');
  assert.equal(a.charAt(2), EM.charAt(2), 'charAt on URL works.');
  assert.equal(a.charCodeAt(2), EM.charCodeAt(2), 'charCodeAt on URL works.');
  assert.equal(a.concat(EM), EM.concat(a), 'concat on URL works.');
  assert.equal(a.substr(2,3), EM.substr(2,3), 'substr on URL works.');
  assert.equal(a.substring(2,3), EM.substring(2,3), 'substring on URL works.');
  assert.equal(a.trim(), EM.trim(), 'trim on URL works.');
  assert.equal(a.trimRight(), EM.trimRight(), 'trimRight on URL works.');
  assert.equal(a.trimLeft(), EM.trimLeft(), 'trimLeft on URL works.');
}

exports.testDataURLwithouthURI = function (assert) {
  let dataURL = new DataURL();

  assert.equal(dataURL.base64, false, 'base64 is false for empty uri')
  assert.equal(dataURL.data, '', 'data is an empty string for empty uri')
  assert.equal(dataURL.mimeType, '', 'mimeType is an empty string for empty uri')
  assert.equal(Object.keys(dataURL.parameters).length, 0, 'parameters is an empty object for empty uri');

  assert.equal(dataURL.toString(), 'data:,');
}

exports.testDataURLwithMalformedURI = function (assert) {
  assert.throws(function() {
      let dataURL = new DataURL('http://www.mozilla.com/');
    },
    /Malformed Data URL: http:\/\/www.mozilla.com\//i,
    'DataURL raises an exception for malformed data uri'
  );
}

exports.testDataURLparse = function (assert) {
  let dataURL = new DataURL('data:text/html;charset=US-ASCII,%3Ch1%3EHello!%3C%2Fh1%3E');

  assert.equal(dataURL.base64, false, 'base64 is false for non base64 data uri')
  assert.equal(dataURL.data, '<h1>Hello!</h1>', 'data is properly decoded')
  assert.equal(dataURL.mimeType, 'text/html', 'mimeType is set properly')
  assert.equal(Object.keys(dataURL.parameters).length, 1, 'one parameters specified');
  assert.equal(dataURL.parameters['charset'], 'US-ASCII', 'charset parsed');

  assert.equal(dataURL.toString(), 'data:text/html;charset=US-ASCII,%3Ch1%3EHello!%3C%2Fh1%3E');
}

exports.testDataURLparseBase64 = function (assert) {
  let text = 'Awesome!';
  let b64text = 'QXdlc29tZSE=';
  let dataURL = new DataURL('data:text/plain;base64,' + b64text);

  assert.equal(dataURL.base64, true, 'base64 is true for base64 encoded data uri')
  assert.equal(dataURL.data, text, 'data is properly decoded')
  assert.equal(dataURL.mimeType, 'text/plain', 'mimeType is set properly')
  assert.equal(Object.keys(dataURL.parameters).length, 1, 'one parameters specified');
  assert.equal(dataURL.parameters['base64'], '', 'parameter set without value');
  assert.equal(dataURL.toString(), 'data:text/plain;base64,' + encodeURIComponent(b64text));
}

exports.testIsValidURI = function (assert) {
  validURIs().forEach(function (aUri) {
    assert.equal(isValidURI(aUri), true, aUri + ' is a valid URL');
  });
};

exports.testIsInvalidURI = function (assert) {
  invalidURIs().forEach(function (aUri) {
    assert.equal(isValidURI(aUri), false, aUri + ' is an invalid URL');
  });
};

exports.testURLFromURL = function(assert) {
  let aURL = URL('http://mozilla.org');
  let bURL = URL(aURL);
  assert.equal(aURL.toString(), bURL.toString(), 'Making a URL from a URL works');
};

exports.testTLD = function(assert) {
  let urls = [
    { url: 'http://my.sub.domains.mozilla.co.uk', tld: 'co.uk' },
    { url: 'http://my.mozilla.com', tld: 'com' },
    { url: 'http://my.domains.mozilla.org.hk', tld: 'org.hk' },
    { url: 'chrome://global/content/blah', tld: 'global' },
    { url: 'data:text/plain;base64,QXdlc29tZSE=', tld: null },
    { url: 'https://1.2.3.4', tld: null }
  ];

  urls.forEach(function (uri) {
    assert.equal(getTLD(uri.url), uri.tld);
    assert.equal(getTLD(URL(uri.url)), uri.tld);
  });
}

exports.testWindowLocationMatch = function (assert, done) {
  let server = httpd.startServerAsync(port);
  server.registerPathHandler('/index.html', function (request, response) {
    response.write('<html><head></head><body><h1>url tests</h1></body></html>');
  });

  let aUrl = 'http://localhost:' + port + '/index.html?q=aQuery#somehash';
  let urlObject = URL(aUrl);

  tabs.open({
    url: aUrl,
    onReady: function (tab) {
      tab.attach({
        onMessage: function (loc) {
          for (let prop in loc) {
            assert.equal(urlObject[prop], loc[prop], prop + ' matches');
          }

          tab.close(function() server.stop(done));
        },
        contentScript: '(' + function () {
          let res = {};
          // `origin` is `null` in this context???
          let props = 'hostname,port,pathname,hash,href,protocol,search'.split(',');
          props.forEach(function (prop) {
            res[prop] = window.location[prop];
          });
          self.postMessage(res);
        } + ')()'
      });
    }
  })
};

function validURIs() {
  return [
  'http://foo.com/blah_blah',
  'http://foo.com/blah_blah/',
  'http://foo.com/blah_blah_(wikipedia)',
  'http://foo.com/blah_blah_(wikipedia)_(again)',
  'http://www.example.com/wpstyle/?p=364',
  'https://www.example.com/foo/?bar=baz&amp;inga=42&amp;quux',
  'http://✪df.ws/123',
  'http://userid:password@example.com:8080',
  'http://userid:password@example.com:8080/',
  'http://userid@example.com',
  'http://userid@example.com/',
  'http://userid@example.com:8080',
  'http://userid@example.com:8080/',
  'http://userid:password@example.com',
  'http://userid:password@example.com/',
  'http://142.42.1.1/',
  'http://142.42.1.1:8080/',
  'http://➡.ws/䨹',
  'http://⌘.ws',
  'http://⌘.ws/',
  'http://foo.com/blah_(wikipedia)#cite-1',
  'http://foo.com/blah_(wikipedia)_blah#cite-1',
  'http://foo.com/unicode_(✪)_in_parens',
  'http://foo.com/(something)?after=parens',
  'http://☺.damowmow.com/',
  'http://code.google.com/events/#&amp;product=browser',
  'http://j.mp',
  'ftp://foo.bar/baz',
  'http://foo.bar/?q=Test%20URL-encoded%20stuff',
  'http://مثال.إختبار',
  'http://例子.测试',
  'http://उदाहरण.परीक्षा',
  'http://-.~_!$&amp;\'()*+,;=:%40:80%2f::::::@example.com',
  'http://1337.net',
  'http://a.b-c.de',
  'http://223.255.255.254',
  // Also want to validate data-uris, localhost
  'http://localhost:8432/some-file.js',
  'data:text/plain;base64,',
  'data:text/html;charset=US-ASCII,%3Ch1%3EHello!%3C%2Fh1%3E',
  'data:text/html;charset=utf-8,'
  ];
}

// Some invalidURIs are valid according to the regex used,
// can be improved in the future, but better to pass some
// invalid URLs than prevent valid URLs

function invalidURIs () {
  return [
//  'http://',
//  'http://.',
//  'http://..',
//  'http://../',
//  'http://?',
//  'http://??',
//  'http://??/',
//  'http://#',
//  'http://##',
//  'http://##/',
//  'http://foo.bar?q=Spaces should be encoded',
  'not a url',
  '//',
  '//a',
  '///a',
  '///',
//  'http:///a',
  'foo.com',
  'http:// shouldfail.com',
  ':// should fail',
//  'http://foo.bar/foo(bar)baz quux',
//  'http://-error-.invalid/',
//  'http://a.b--c.de/',
//  'http://-a.b.co',
//  'http://a.b-.co',
//  'http://0.0.0.0',
//  'http://10.1.1.0',
//  'http://10.1.1.255',
//  'http://224.1.1.1',
//  'http://1.1.1.1.1',
//  'http://123.123.123',
//  'http://3628126748',
//  'http://.www.foo.bar/',
//  'http://www.foo.bar./',
//  'http://.www.foo.bar./',
//  'http://10.1.1.1',
//  'http://10.1.1.254'
  ];
}

require('sdk/test').run(exports);
