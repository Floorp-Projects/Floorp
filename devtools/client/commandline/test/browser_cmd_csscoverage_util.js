/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

const csscoverage = require("devtools/server/actors/csscoverage");

add_task(function*() {
  testDeconstructRuleId();
});

function testDeconstructRuleId() {
  // This is the easy case
  let rule = csscoverage.deconstructRuleId("http://thing/blah|10|20");
  is(rule.url, "http://thing/blah", "1 url");
  is(rule.line, 10, "1 line");
  is(rule.column, 20, "1 column");

  // This is the harder case with a URL containing a '|'
  rule = csscoverage.deconstructRuleId("http://thing/blah?q=a|b|11|22");
  is(rule.url, "http://thing/blah?q=a|b", "2 url");
  is(rule.line, 11, "2 line");
  is(rule.column, 22, "2 column");
}
