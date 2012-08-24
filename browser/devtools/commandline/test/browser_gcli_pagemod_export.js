/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inspect command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/test/browser_gcli_inspect.html";

function test() {
  let initialHtml = "";

  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    initialHtml = content.document.documentElement.innerHTML;

    testExportHtml();
    testPageModReplace();
    testPageModRemoveElement();
    testPageModRemoveAttribute();
    finish();
  });

  function testExportHtml() {
    DeveloperToolbarTest.checkInputStatus({
      typed: "export html",
      status: "VALID"
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
    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod replace",
      emptyParameters: [" <search>", " <replace>", " [ignoreCase]",
                        " [selector]", " [root]", " [attrOnly]",
                        " [contentOnly]", " [attributes]"],
      status: "ERROR"
    });

    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod replace some foo",
      emptyParameters: [" [ignoreCase]", " [selector]", " [root]",
                        " [attrOnly]", " [contentOnly]", " [attributes]"],
      status: "VALID"
    });

    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod replace some foo true",
      emptyParameters: [" [selector]", " [root]", " [attrOnly]",
                        " [contentOnly]", " [attributes]"],
      status: "VALID"
    });

    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod replace some foo true --attrOnly",
      emptyParameters: [" [selector]", " [root]", " [contentOnly]",
                        " [attributes]"],
      status: "VALID"
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
    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod remove",
      status: "ERROR"
    });

    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod remove element",
      emptyParameters: [" <search>", " [root]", " [stripOnly]", " [ifEmptyOnly]"],
      status: "ERROR"
    });

    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod remove element foo",
      emptyParameters: [" [root]", " [stripOnly]", " [ifEmptyOnly]"],
      status: "VALID"
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
    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod remove attribute",
      emptyParameters: [" <searchAttributes>", " <searchElements>", " [root]", " [ignoreCase]"],
      status: "ERROR"
    });

    DeveloperToolbarTest.checkInputStatus({
      typed: "pagemod remove attribute foo bar",
      emptyParameters: [" [root]", " [ignoreCase]"],
      status: "VALID"
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
