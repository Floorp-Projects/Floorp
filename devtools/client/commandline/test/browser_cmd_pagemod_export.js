/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inspect command works as it should

const TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_pagemod_export.html";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  function getHTML() {
    return ContentTask.spawn(options.browser, {}, function* () {
      return content.document.documentElement.innerHTML;
    });
  }

  const initialHtml = yield getHTML();

  function resetContent() {
    return ContentTask.spawn(options.browser, initialHtml, function* (html) {
      content.document.documentElement.innerHTML = html;
    });
  }

  // Test exporting HTML
  yield ContentTask.spawn(options.browser, {}, function* () {
    content.wrappedJSObject.oldOpen = content.open;
    content.wrappedJSObject.openURL = "";
    content.wrappedJSObject.open = function (url) {
      // The URL is a data: URL that contains the document source
      content.wrappedJSObject.openURL = decodeURIComponent(url);
    };
  });

  yield helpers.audit(options, [
    {
      setup:    "export html",
      skipIf: true,
      check: {
        input:  "export html",
        hints:             " [destination]",
        markup: "VVVVVVVVVVV",
        status: "VALID",
      },
      exec: {
        output: ""
      },
      post: Task.async(function* () {
        yield ContentTask.spawn(options.browser, {}, function* () {
          let openURL =  content.wrappedJSObject.openURL;
          isnot(openURL.indexOf('<html lang="en">'), -1, "export html works: <html>");
          isnot(openURL.indexOf("<title>GCLI"), -1, "export html works: <title>");
          isnot(openURL.indexOf('<p id="someid">#'), -1, "export html works: <p>");
        });
      })
    },
    {
      setup:    "export html stdout",
      check: {
        input:  "export html stdout",
        hints:                    "",
        markup: "VVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          destination: { value: "stdout" }
        },
      },
      exec: {
        output: [
          /<html lang="en">/,
          /<title>GCLI/,
          /<p id="someid">#/
        ]
      }
    }
  ]);

  yield ContentTask.spawn(options.browser, {}, function* () {
    content.wrappedJSObject.open = content.wrappedJSObject.oldOpen;
    delete content.wrappedJSObject.openURL;
    delete content.wrappedJSObject.oldOpen;
  });

  // Test 'pagemod replace'
  yield helpers.audit(options, [
    {
      setup: "pagemod replace",
      check: {
        input:  "pagemod replace",
        hints:                 " <search> <replace> [ignoreCase] [selector] [root] [attrOnly] [contentOnly] [attributes]",
        markup: "VVVVVVVVVVVVVVV",
        status: "ERROR"
      }
    },
    {
      setup: "pagemod replace some foo",
      check: {
        input:  "pagemod replace some foo",
        hints:                          " [ignoreCase] [selector] [root] [attrOnly] [contentOnly] [attributes]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      }
    },
    {
      setup: "pagemod replace some foo true",
      check: {
        input:  "pagemod replace some foo true",
        hints:                               " [selector] [root] [attrOnly] [contentOnly] [attributes]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      }
    },
    {
      setup: "pagemod replace some foo true --attrOnly",
      check: {
        input:  "pagemod replace some foo true --attrOnly",
        hints:                                          " [selector] [root] [contentOnly] [attributes]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      }
    },
    {
      setup: "pagemod replace sOme foOBar",
      exec: {
        output: /^[^:]+: 13\. [^:]+: 0\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "no change in the page");
      })
    },
    {
      setup: "pagemod replace sOme foOBar true",
      exec: {
        output: /^[^:]+: 13\. [^:]+: 2\. [^:]+: 2\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        isnot(html.indexOf('<p class="foOBarclass">.foOBarclass'), -1,
              ".someclass changed to .foOBarclass");
        isnot(html.indexOf('<p id="foOBarid">#foOBarid'), -1,
              "#someid changed to #foOBarid");

        yield resetContent();
      })
    },
    {
      setup: "pagemod replace some foobar --contentOnly",
      exec: {
        output: /^[^:]+: 13\. [^:]+: 2\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        isnot(html.indexOf('<p class="someclass">.foobarclass'), -1,
              ".someclass changed to .foobarclass (content only)");
        isnot(html.indexOf('<p id="someid">#foobarid'), -1,
              "#someid changed to #foobarid (content only)");

        yield resetContent();
      })
    },
    {
      setup: "pagemod replace some foobar --attrOnly",
      exec: {
        output: /^[^:]+: 13\. [^:]+: 0\. [^:]+: 2\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        isnot(html.indexOf('<p class="foobarclass">.someclass'), -1,
              ".someclass changed to .foobarclass (attr only)");
        isnot(html.indexOf('<p id="foobarid">#someid'), -1,
              "#someid changed to #foobarid (attr only)");

        yield resetContent();
      })
    },
    {
      setup: "pagemod replace some foobar --root head",
      exec: {
        output: /^[^:]+: 2\. [^:]+: 0\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "nothing changed");
      })
    },
    {
      setup: "pagemod replace some foobar --selector .someclass,div,span",
      exec: {
        output: /^[^:]+: 4\. [^:]+: 1\. [^:]+: 1\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        isnot(html.indexOf('<p class="foobarclass">.foobarclass'), -1,
              ".someclass changed to .foobarclass");
        isnot(html.indexOf('<p id="someid">#someid'), -1,
              "#someid did not change");

        yield resetContent();
      })
    },
  ]);

  // Test 'pagemod remove element'
  yield helpers.audit(options, [
    {
      setup: "pagemod remove",
      check: {
        input:  "pagemod remove",
        hints:                " attribute",
        markup: "IIIIIIIVIIIIII",
        status: "ERROR"
      },
    },
    {
      setup: "pagemod remove element",
      check: {
        input:  "pagemod remove element",
        hints:                        " <search> [root] [stripOnly] [ifEmptyOnly]",
        markup: "VVVVVVVVVVVVVVVVVVVVVV",
        status: "ERROR"
      },
    },
    {
      setup: "pagemod remove element foo",
      check: {
        input:  "pagemod remove element foo",
        hints:                            " [root] [stripOnly] [ifEmptyOnly]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
    },
    {
      setup: "pagemod remove element p",
      exec: {
        output: /^[^:]+: 3\. [^:]+: 3\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        is(html.indexOf('<p class="someclass">'), -1, "p.someclass removed");
        is(html.indexOf('<p id="someid">'), -1, "p#someid removed");
        is(html.indexOf("<p><strong>"), -1, "<p> wrapping <strong> removed");
        isnot(html.indexOf("<span>"), -1, "<span> not removed");

        yield resetContent();
      })
    },
    {
      setup: "pagemod remove element p head",
      exec: {
        output: /^[^:]+: 0\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "nothing changed in the page");
      })
    },
    {
      setup: "pagemod remove element p --ifEmptyOnly",
      exec: {
        output: /^[^:]+: 3\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "nothing changed in the page");
      })
    },
    {
      setup: "pagemod remove element meta,title --ifEmptyOnly",
      exec: {
        output: /^[^:]+: 2\. [^:]+: 1\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        is(html.indexOf("<meta charset="), -1, "<meta> removed");
        isnot(html.indexOf("<title>"), -1, "<title> not removed");

        yield resetContent();
      })
    },
    {
      setup: "pagemod remove element p --stripOnly",
      exec: {
        output: /^[^:]+: 3\. [^:]+: 3\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        is(html.indexOf('<p class="someclass">'), -1, "p.someclass removed");
        is(html.indexOf('<p id="someid">'), -1, "p#someid removed");
        is(html.indexOf("<p><strong>"), -1, "<p> wrapping <strong> removed");
        isnot(html.indexOf(".someclass"), -1, ".someclass still exists");
        isnot(html.indexOf("#someid"), -1, "#someid still exists");
        isnot(html.indexOf("<strong>p"), -1, "<strong> still exists");

        yield resetContent();
      })
    },
  ]);

  // Test 'pagemod remove attribute'
  yield helpers.audit(options, [
    {
      setup: "pagemod remove attribute",
      check: {
        input:  "pagemod remove attribute",
        hints:                          " <searchAttributes> <searchElements> [root] [ignoreCase]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVV",
        status: "ERROR",
        args: {
          searchAttributes: { value: undefined, status: "INCOMPLETE" },
          searchElements: { value: undefined, status: "INCOMPLETE" },
          // root: { value: undefined }, // 'root' is a node which is remote
                                         // so we can't see the value in tests
          ignoreCase: { value: false },
        }
      },
    },
    {
      setup: "pagemod remove attribute foo bar",
      check: {
        input:  "pagemod remove attribute foo bar",
        hints:                                  " [root] [ignoreCase]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          searchAttributes: { value: "foo" },
          searchElements: { value: "bar" },
          // root: { value: undefined }, // 'root' is a node which is remote
                                         // so we can't see the value in tests
          ignoreCase: { value: false },
        }
      },
      post: function () {
        return new Promise(resolve => {
          executeSoon(resolve);
        });
      }
    },
    {
      setup: "pagemod remove attribute foo bar",
      exec: {
        output: /^[^:]+: 0\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "nothing changed in the page");
      })
    },
    {
      setup: "pagemod remove attribute foo p",
      exec: {
        output: /^[^:]+: 3\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "nothing changed in the page");
      })
    },
    {
      setup: "pagemod remove attribute id p,span",
      exec: {
        output: /^[^:]+: 5\. [^:]+: 1\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        is(html.indexOf('<p id="someid">#someid'), -1, "p#someid attribute removed");
        isnot(html.indexOf("<p>#someid"), -1, "p with someid content still exists");

        yield resetContent();
      })
    },
    {
      setup: "pagemod remove attribute Class p",
      exec: {
        output: /^[^:]+: 3\. [^:]+: 0\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();
        is(html, initialHtml, "nothing changed in the page");
      })
    },
    {
      setup: "pagemod remove attribute Class p --ignoreCase",
      exec: {
        output: /^[^:]+: 3\. [^:]+: 1\.\s*$/
      },
      post: Task.async(function* () {
        let html = yield getHTML();

        is(html.indexOf('<p class="someclass">.someclass'), -1,
           "p.someclass attribute removed");
        isnot(html.indexOf("<p>.someclass"), -1,
           "p with someclass content still exists");

        yield resetContent();
      })
    },
  ]);

  // Shutdown
  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
