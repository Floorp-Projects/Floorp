/**
 * This file lists the tests in the BrowserScope suite which we are currently
 * failing.  We mark them as todo items to keep track of them.
 */

var knownFailures = {
  // Dummy result items.  There is one for each category.
  'apply' : {
    '0-undefined' : true
  },
  'unapply' : {
    '0-undefined' : true
  },
  'change' : {
    '0-undefined' : true
  },
  'query' : {
    '0-undefined' : true
  },
  'a' : {
    'createbookmark-0' : true,
    'decreasefontsize-0' : true,
    'fontsize-1' : true,
    'subscript-1' : true,
    'superscript-1' : true,
  },
  'u': {
    'removeformat-1' : true,
    'removeformat-2' : true,
    'strikethrough-2' : true,
    'subscript-1' : true,
    'superscript-1' : true,
    'unbookmark-0' : true,
  },
  'q': {
    'fontsize-1' : true,
    'fontsize-2' : true,
  },
  'c': {
    'fontsize-1' : true,
    'fontsize-2' : true,
  },
};

function isKnownFailure(type, test, param) {
  return (type in knownFailures) && knownFailures[type][test + "-" + param];
}
