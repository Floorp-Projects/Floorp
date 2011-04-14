const Cc = Components.classes;
const Ci = Components.interfaces;

var timer;

// This test XPConnect's ability to deal with a certain type of exception. In
// particular, bug 641378 meant that if an exception had both 'message' and
// 'result' properties, then it wouldn't successfully read the 'result' field
// out of the exception (and sometimes crash).
//
// In order to make the test not fail completely on a negative result, we use
// a timer. The first time through the timer, we throw our special exception.
// Then, the second time through, we can test to see if XPConnect properly
// dealt with our exception.
var exception = {
  message: "oops, something failed!",

  tries: 0,
  get result() {
    ++this.tries;
    return 3;
  }
};

var callback = {
  tries: 0,
  notify: function (timer) {
    if (++this.tries === 1)
      throw exception;

    try {
        do_check_true(exception.tries >= 1);
    } finally {
        timer.cancel();
        timer = null;
        do_test_finished();
    }
  }
};

function run_test() {
  do_test_pending();

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(callback, 0, timer.TYPE_REPEATING_SLACK);
}
