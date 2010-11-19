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
    'backcolor-0' : true,
    'backcolor-1' : true,
    'createbookmark-0' : true,
    'fontsize-1' : true,
    'hilitecolor-0' : true,
    'subscript-1' : true,
    'superscript-1' : true,
  },
  'u': {
    'bold-1' : true,
    'italic-1' : true,
    'removeformat-1' : true,
    'removeformat-2' : true,
    'strikethrough-1' : true,
    'strikethrough-2' : true,
    'subscript-1' : true,
    'superscript-1' : true,
    'unbookmark-0' : true,
  },
  'q': {
    'backcolor-0' : true,
    'backcolor-1' : true,
    'backcolor-2' : true,
    'fontsize-1' : true,
    'fontsize-2' : true,
  },
  'c': {
    'backcolor-0' : true,
    'backcolor-1' : true,
    'backcolor-2' : true,
    'fontname-0' : true,
    'fontname-2' : true,
    'fontname-3' : true,
    'fontsize-1' : true,
    'fontsize-2' : true,
    'forecolor-0' : true,
    'forecolor-2' : true,
  },
};

function isKnownFailure(type, test, param) {
  return (type in knownFailures) && ((test + "-" + param) in knownFailures[type]);
}
