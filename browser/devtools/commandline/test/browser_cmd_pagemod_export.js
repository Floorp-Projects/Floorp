/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inspect command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/"+
                 "test/browser_cmd_pagemod_export.html";

function test() {
  let initialHtml = "";

  var tests = {};

  helpers.addTabWithToolbar(TEST_URI, function(options) {
    initialHtml = content.document.documentElement.innerHTML;

    return helpers.runTests(options, tests);
  }).then(finish);

  function getContent(options) {
    return options.document.documentElement.innerHTML;
  }

  function resetContent(options) {
    options.document.documentElement.innerHTML = initialHtml;
  }

  tests.testExportHtml = function(options) {
    let oldOpen = options.window.open;
    let openURL = "";
    options.window.open = function(url) {
      // The URL is a data: URL that contains the document source
      openURL = decodeURIComponent(url);
    };

    return helpers.audit(options, [
      {
        setup: 'export html',
        check: {
          input:  'export html',
          hints:             '',
          markup: 'VVVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: ''
        },
        post: function() {
          isnot(openURL.indexOf('<html lang="en">'), -1, "export html works: <html>");
          isnot(openURL.indexOf("<title>GCLI"), -1, "export html works: <title>");
          isnot(openURL.indexOf('<p id="someid">#'), -1, "export html works: <p>");

           options.window.open = oldOpen;
        }
      }
    ]);
  };

  tests.testPageModReplace = function(options) {
    return helpers.audit(options, [
      {
        setup: 'pagemod replace',
        check: {
          input:  'pagemod replace',
          hints:                 ' <search> <replace> [ignoreCase] [selector] [root] [attrOnly] [contentOnly] [attributes]',
          markup: 'VVVVVVVVVVVVVVV',
          status: 'ERROR'
        }
      },
      {
        setup: 'pagemod replace some foo',
        check: {
          input:  'pagemod replace some foo',
          hints:                          ' [ignoreCase] [selector] [root] [attrOnly] [contentOnly] [attributes]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'pagemod replace some foo true',
        check: {
          input:  'pagemod replace some foo true',
          hints:                               ' [selector] [root] [attrOnly] [contentOnly] [attributes]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'pagemod replace some foo true --attrOnly',
        check: {
          input:  'pagemod replace some foo true --attrOnly',
          hints:                                          ' [selector] [root] [contentOnly] [attributes]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'pagemod replace sOme foOBar',
        exec: {
          output: /^[^:]+: 13\. [^:]+: 0\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "no change in the page");
        }
      },
      {
        setup: 'pagemod replace sOme foOBar true',
        exec: {
          output: /^[^:]+: 13\. [^:]+: 2\. [^:]+: 2\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          isnot(html.indexOf('<p class="foOBarclass">.foOBarclass'), -1,
                ".someclass changed to .foOBarclass");
          isnot(html.indexOf('<p id="foOBarid">#foOBarid'), -1,
                "#someid changed to #foOBarid");

          resetContent(options);
        }
      },
      {
        setup: 'pagemod replace some foobar --contentOnly',
        exec: {
          output: /^[^:]+: 13\. [^:]+: 2\. [^:]+: 0\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          isnot(html.indexOf('<p class="someclass">.foobarclass'), -1,
                ".someclass changed to .foobarclass (content only)");
          isnot(html.indexOf('<p id="someid">#foobarid'), -1,
                "#someid changed to #foobarid (content only)");

          resetContent(options);
        }
      },
      {
        setup: 'pagemod replace some foobar --attrOnly',
        exec: {
          output: /^[^:]+: 13\. [^:]+: 0\. [^:]+: 2\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          isnot(html.indexOf('<p class="foobarclass">.someclass'), -1,
                ".someclass changed to .foobarclass (attr only)");
          isnot(html.indexOf('<p id="foobarid">#someid'), -1,
                "#someid changed to #foobarid (attr only)");

          resetContent(options);
        }
      },
      {
        setup: 'pagemod replace some foobar --root head',
        exec: {
          output: /^[^:]+: 2\. [^:]+: 0\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "nothing changed");
        }
      },
      {
        setup: 'pagemod replace some foobar --selector .someclass,div,span',
        exec: {
          output: /^[^:]+: 4\. [^:]+: 1\. [^:]+: 1\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          isnot(html.indexOf('<p class="foobarclass">.foobarclass'), -1,
                ".someclass changed to .foobarclass");
          isnot(html.indexOf('<p id="someid">#someid'), -1,
                "#someid did not change");

          resetContent(options);
        }
      },
    ]);
  };

  tests.testPageModRemoveElement = function(options) {
    return helpers.audit(options, [
      {
        setup: 'pagemod remove',
        check: {
          input:  'pagemod remove',
          hints:                ' attribute',
          markup: 'IIIIIIIVIIIIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'pagemod remove element',
        check: {
          input:  'pagemod remove element',
          hints:                        ' <search> [root] [stripOnly] [ifEmptyOnly]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'pagemod remove element foo',
        check: {
          input:  'pagemod remove element foo',
          hints:                            ' [root] [stripOnly] [ifEmptyOnly]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        },
      },
      {
        setup: 'pagemod remove element p',
        exec: {
          output: /^[^:]+: 3\. [^:]+: 3\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          is(html.indexOf('<p class="someclass">'), -1, "p.someclass removed");
          is(html.indexOf('<p id="someid">'), -1, "p#someid removed");
          is(html.indexOf("<p><strong>"), -1, "<p> wrapping <strong> removed");
          isnot(html.indexOf("<span>"), -1, "<span> not removed");

          resetContent(options);
        }
      },
      {
        setup: 'pagemod remove element p head',
        exec: {
          output: /^[^:]+: 0\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "nothing changed in the page");
        }
      },
      {
        setup: 'pagemod remove element p --ifEmptyOnly',
        exec: {
          output: /^[^:]+: 3\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "nothing changed in the page");
        }
      },
      {
        setup: 'pagemod remove element meta,title --ifEmptyOnly',
        exec: {
          output: /^[^:]+: 2\. [^:]+: 1\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          is(html.indexOf("<meta charset="), -1, "<meta> removed");
          isnot(html.indexOf("<title>"), -1, "<title> not removed");

          resetContent(options);
        }
      },
      {
        setup: 'pagemod remove element p --stripOnly',
        exec: {
          output: /^[^:]+: 3\. [^:]+: 3\.\s*$/
        },
        post: function() {
          let html = getContent(options);

          is(html.indexOf('<p class="someclass">'), -1, "p.someclass removed");
          is(html.indexOf('<p id="someid">'), -1, "p#someid removed");
          is(html.indexOf("<p><strong>"), -1, "<p> wrapping <strong> removed");
          isnot(html.indexOf(".someclass"), -1, ".someclass still exists");
          isnot(html.indexOf("#someid"), -1, "#someid still exists");
          isnot(html.indexOf("<strong>p"), -1, "<strong> still exists");

          resetContent(options);
        }
      },
    ]);
  };

  tests.testPageModRemoveAttribute = function(options) {
    return helpers.audit(options, [
      {
        setup: 'pagemod remove attribute',
        check: {
          input:  'pagemod remove attribute',
          hints:                          ' <searchAttributes> <searchElements> [root] [ignoreCase]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'ERROR',
          args: {
            searchAttributes: { value: undefined, status: 'INCOMPLETE' },
            searchElements: { value: undefined, status: 'INCOMPLETE' },
            root: { value: undefined },
            ignoreCase: { value: false },
          }
        },
      },
      {
        setup: 'pagemod remove attribute foo bar',
        check: {
          input:  'pagemod remove attribute foo bar',
          hints:                                  ' [root] [ignoreCase]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            searchAttributes: { value: 'foo' },
            searchElements: { value: 'bar' },
            root: { value: undefined },
            ignoreCase: { value: false },
          }
        },
        post: function() {
          let deferred = promise.defer();
          executeSoon(function() {
            deferred.resolve();
          });
          return deferred.promise;
        }
      },
      {
        setup: 'pagemod remove attribute foo bar',
        exec: {
          output: /^[^:]+: 0\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "nothing changed in the page");
        }
      },
      {
        setup: 'pagemod remove attribute foo p',
        exec: {
          output: /^[^:]+: 3\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "nothing changed in the page");
        }
      },
      {
        setup: 'pagemod remove attribute id p,span',
        exec: {
          output: /^[^:]+: 5\. [^:]+: 1\.\s*$/
        },
        post: function() {
          is(getContent(options).indexOf('<p id="someid">#someid'), -1,
             "p#someid attribute removed");
          isnot(getContent(options).indexOf("<p>#someid"), -1,
             "p with someid content still exists");

          resetContent(options);
        }
      },
      {
        setup: 'pagemod remove attribute Class p',
        exec: {
          output: /^[^:]+: 3\. [^:]+: 0\.\s*$/
        },
        post: function() {
          is(getContent(options), initialHtml, "nothing changed in the page");
        }
      },
      {
        setup: 'pagemod remove attribute Class p --ignoreCase',
        exec: {
          output: /^[^:]+: 3\. [^:]+: 1\.\s*$/
        },
        post: function() {
          is(getContent(options).indexOf('<p class="someclass">.someclass'), -1,
             "p.someclass attribute removed");
          isnot(getContent(options).indexOf("<p>.someclass"), -1,
             "p with someclass content still exists");

          resetContent(options);
        }
      },
    ]);
  };
}
