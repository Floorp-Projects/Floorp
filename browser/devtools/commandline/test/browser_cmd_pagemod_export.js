/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inspect command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/"+
                 "test/browser_cmd_pagemod_export.html";

function test() {
  let initialHtml = "";

  DeveloperToolbarTest.test(TEST_URI, [
    init,
    testExportHtml,
    testPageModReplace,
    testPageModRemoveElement,
    testPageModRemoveAttribute
  ]);

  function init() {
    initialHtml = content.document.documentElement.innerHTML;
  }

  function testExportHtml() {
    helpers.setInput('export html');
    helpers.check({
      input:  'export html',
      hints:             '',
      markup: 'VVVVVVVVVVV',
      status: 'VALID'
    });

    let oldOpen = content.open;
    let openURL = "";
    content.open = function(aUrl) {
      openURL = aUrl;
    };

    DeveloperToolbarTest.exec({ blankOutput: true });

    openURL = decodeURIComponent(openURL);

    isnot(openURL.indexOf('<html lang="en">'), -1, "export html works: <html>");
    isnot(openURL.indexOf("<title>GCLI"), -1, "export html works: <title>");
    isnot(openURL.indexOf('<p id="someid">#'), -1, "export html works: <p>");

    content.open = oldOpen;
  }

  function getContent() {
    return content.document.documentElement.innerHTML;
  }

  function resetContent() {
    content.document.documentElement.innerHTML = initialHtml;
  }

  function testPageModReplace() {
    helpers.setInput('pagemod replace');
    helpers.check({
      input:  'pagemod replace',
      hints:                 ' <search> <replace> [ignoreCase] [selector] [root] [attrOnly] [contentOnly] [attributes]',
      markup: 'VVVVVVVVVVVVVVV',
      status: 'ERROR'
    });

    helpers.setInput('pagemod replace some foo');
    helpers.check({
      input:  'pagemod replace some foo',
      hints:                          ' [ignoreCase] [selector] [root] [attrOnly] [contentOnly] [attributes]',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('pagemod replace some foo true');
    helpers.check({
      input:  'pagemod replace some foo true',
      hints:                               ' [selector] [root] [attrOnly] [contentOnly] [attributes]',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('pagemod replace some foo true --attrOnly');
    helpers.check({
      input:  'pagemod replace some foo true --attrOnly',
      hints:                                          ' [selector] [root] [contentOnly] [attributes]',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    DeveloperToolbarTest.exec({
      typed: "pagemod replace sOme foOBar",
      outputMatch: /^[^:]+: 13\. [^:]+: 0\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "no change in the page");

    DeveloperToolbarTest.exec({
      typed: "pagemod replace sOme foOBar true",
      outputMatch: /^[^:]+: 13\. [^:]+: 2\. [^:]+: 2\.\s*$/
    });

    isnot(getContent().indexOf('<p class="foOBarclass">.foOBarclass'), -1,
          ".someclass changed to .foOBarclass");
    isnot(getContent().indexOf('<p id="foOBarid">#foOBarid'), -1,
          "#someid changed to #foOBarid");

    resetContent();

    DeveloperToolbarTest.exec({
      typed: "pagemod replace some foobar --contentOnly",
      outputMatch: /^[^:]+: 13\. [^:]+: 2\. [^:]+: 0\.\s*$/
    });

    isnot(getContent().indexOf('<p class="someclass">.foobarclass'), -1,
          ".someclass changed to .foobarclass (content only)");
    isnot(getContent().indexOf('<p id="someid">#foobarid'), -1,
          "#someid changed to #foobarid (content only)");

    resetContent();

    DeveloperToolbarTest.exec({
      typed: "pagemod replace some foobar --attrOnly",
      outputMatch: /^[^:]+: 13\. [^:]+: 0\. [^:]+: 2\.\s*$/
    });

    isnot(getContent().indexOf('<p class="foobarclass">.someclass'), -1,
          ".someclass changed to .foobarclass (attr only)");
    isnot(getContent().indexOf('<p id="foobarid">#someid'), -1,
          "#someid changed to #foobarid (attr only)");

    resetContent();

    DeveloperToolbarTest.exec({
      typed: "pagemod replace some foobar --root head",
      outputMatch: /^[^:]+: 2\. [^:]+: 0\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "nothing changed");

    DeveloperToolbarTest.exec({
      typed: "pagemod replace some foobar --selector .someclass,div,span",
      outputMatch: /^[^:]+: 4\. [^:]+: 1\. [^:]+: 1\.\s*$/
    });

    isnot(getContent().indexOf('<p class="foobarclass">.foobarclass'), -1,
          ".someclass changed to .foobarclass");
    isnot(getContent().indexOf('<p id="someid">#someid'), -1,
          "#someid did not change");

    resetContent();
  }

  function testPageModRemoveElement() {
    helpers.setInput('pagemod remove');
    helpers.check({
      input:  'pagemod remove',
      hints:                '',
      markup: 'IIIIIIIVIIIIII',
      status: 'ERROR'
    });

    helpers.setInput('pagemod remove element');
    helpers.check({
      input:  'pagemod remove element',
      hints:                        ' <search> [root] [stripOnly] [ifEmptyOnly]',
      markup: 'VVVVVVVVVVVVVVVVVVVVVV',
      status: 'ERROR'
    });

    helpers.setInput('pagemod remove element foo');
    helpers.check({
      input:  'pagemod remove element foo',
      hints:                            ' [root] [stripOnly] [ifEmptyOnly]',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    DeveloperToolbarTest.exec({
      typed: "pagemod remove element p",
      outputMatch: /^[^:]+: 3\. [^:]+: 3\.\s*$/
    });

    is(getContent().indexOf('<p class="someclass">'), -1, "p.someclass removed");
    is(getContent().indexOf('<p id="someid">'), -1, "p#someid removed");
    is(getContent().indexOf("<p><strong>"), -1, "<p> wrapping <strong> removed");
    isnot(getContent().indexOf("<span>"), -1, "<span> not removed");

    resetContent();

    DeveloperToolbarTest.exec({
      typed: "pagemod remove element p head",
      outputMatch: /^[^:]+: 0\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "nothing changed in the page");

    DeveloperToolbarTest.exec({
      typed: "pagemod remove element p --ifEmptyOnly",
      outputMatch: /^[^:]+: 3\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "nothing changed in the page");

    DeveloperToolbarTest.exec({
      typed: "pagemod remove element meta,title --ifEmptyOnly",
      outputMatch: /^[^:]+: 2\. [^:]+: 1\.\s*$/
    });

    is(getContent().indexOf("<meta charset="), -1, "<meta> removed");
    isnot(getContent().indexOf("<title>"), -1, "<title> not removed");

    resetContent();

    DeveloperToolbarTest.exec({
      typed: "pagemod remove element p --stripOnly",
      outputMatch: /^[^:]+: 3\. [^:]+: 3\.\s*$/
    });

    is(getContent().indexOf('<p class="someclass">'), -1, "p.someclass removed");
    is(getContent().indexOf('<p id="someid">'), -1, "p#someid removed");
    is(getContent().indexOf("<p><strong>"), -1, "<p> wrapping <strong> removed");
    isnot(getContent().indexOf(".someclass"), -1, ".someclass still exists");
    isnot(getContent().indexOf("#someid"), -1, "#someid still exists");
    isnot(getContent().indexOf("<strong>p"), -1, "<strong> still exists");

    resetContent();
  }

  function testPageModRemoveAttribute() {
    helpers.setInput('pagemod remove attribute ');
    helpers.check({
      input:  'pagemod remove attribute ',
      hints:                           '<searchAttributes> <searchElements> [root] [ignoreCase]',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'ERROR',
      args: {
        searchAttributes: { value: undefined, status: 'INCOMPLETE' },
        searchElements: { value: undefined, status: 'INCOMPLETE' },
        root: { value: undefined },
        ignoreCase: { value: false },
      }
    });

    helpers.setInput('pagemod remove attribute foo bar');
    helpers.check({
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
    });

    DeveloperToolbarTest.exec({
      typed: "pagemod remove attribute foo bar",
      outputMatch: /^[^:]+: 0\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "nothing changed in the page");

    DeveloperToolbarTest.exec({
      typed: "pagemod remove attribute foo p",
      outputMatch: /^[^:]+: 3\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "nothing changed in the page");

    DeveloperToolbarTest.exec({
      typed: "pagemod remove attribute id p,span",
      outputMatch: /^[^:]+: 5\. [^:]+: 1\.\s*$/
    });

    is(getContent().indexOf('<p id="someid">#someid'), -1,
       "p#someid attribute removed");
    isnot(getContent().indexOf("<p>#someid"), -1,
       "p with someid content still exists");

    resetContent();

    DeveloperToolbarTest.exec({
      typed: "pagemod remove attribute Class p",
      outputMatch: /^[^:]+: 3\. [^:]+: 0\.\s*$/
    });

    is(getContent(), initialHtml, "nothing changed in the page");

    DeveloperToolbarTest.exec({
      typed: "pagemod remove attribute Class p --ignoreCase",
      outputMatch: /^[^:]+: 3\. [^:]+: 1\.\s*$/
    });

    is(getContent().indexOf('<p class="someclass">.someclass'), -1,
       "p.someclass attribute removed");
    isnot(getContent().indexOf("<p>.someclass"), -1,
       "p with someclass content still exists");

    resetContent();
  }
}
