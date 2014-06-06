/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// When strings containing URLs are entered into the webconsole,
// check its output and ensure that the output can be clicked to open those URLs.

const TEST_URI = "data:text/html;charset=utf8,Bug 1005909 - Clickable URLS";

let inputTests = [

  // 0: URL opens page when clicked.
  {
    input: "'http://example.com'",
    output: "http://example.com",
    expectedTab: "http://example.com/",
  },

  // 1: URL opens page using https when clicked.
  {
    input: "'https://example.com'",
    output: "https://example.com",
    expectedTab: "https://example.com/",
  },

  // 2: URL with port opens page when clicked.
  {
    input: "'https://example.com:443'",
    output: "https://example.com:443",
    expectedTab: "https://example.com/",
  },

  // 3: URL containing non-empty path opens page when clicked.
  {
    input: "'http://example.com/foo'",
    output: "http://example.com/foo",
    expectedTab: "http://example.com/foo",
  },

  // 4: URL opens page when clicked, even when surrounded by non-URL tokens.
  {
  	input: "'foo http://example.com bar'",
  	output: "foo http://example.com bar",
  	expectedTab: "http://example.com/",
  },

  // 5: URL opens page when clicked, and whitespace is be preserved.
  {
  	input: "'foo\\nhttp://example.com\\nbar'",
  	output: "foo\nhttp://example.com\nbar",
  	expectedTab: "http://example.com/",
  },

  // 6: URL opens page when clicked when multiple links are present.
  {
  	input: "'http://example.com http://example.com'",
  	output: "http://example.com http://example.com",
  	expectedTab: "http://example.com/",
  },

  // 7: URL without scheme does not open page when clicked.
  {
    input: "'example.com'",
    output: "example.com",
  },

  // 8: URL with invalid scheme does not open page when clicked.
  {
    input: "'foo://example.com'",
    output: "foo://example.com",
  },

];

function test() {
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    yield checkOutputForInputs(hud, inputTests);
    inputTests = null;
  }).then(finishTest);
}
