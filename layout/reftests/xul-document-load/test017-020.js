/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

// check that the second node under the document element is a PI with the
// specified .target and .data
function checkDOM(target, data) {
  // assume there are no whitespace nodes in XUL
  var piNode = document.documentElement.childNodes[1];
  if (!piNode || piNode.nodeType != Node.PROCESSING_INSTRUCTION_NODE ||
      piNode.target != target || piNode.data != data) {
    document.documentElement.style.backgroundColor = "red";
  }
}
