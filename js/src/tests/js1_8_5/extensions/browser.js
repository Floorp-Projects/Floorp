// The page loaded in the browser is jsreftest.html, which is located in
// js/src/tests. That makes Worker script URLs resolve relative to the wrong
// directory. workerDir is the workaround.
workerDir = (document.location.href.replace(/\/[^/?]*(\?.*)?$/, '/') +
             gTestsuite + '/' + gTestsubsuite + '/');

