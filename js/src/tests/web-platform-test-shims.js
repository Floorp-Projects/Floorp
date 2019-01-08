// This is used when jstests.py runs web-platform-tests in the shell.
// Some tests require browser features that the shell doesn't have built-in.

// window.self
// <https://html.spec.whatwg.org/multipage/window-object.html#dom-self>
if (!("self" in this)) {
  this.self = this;
}

// window.setTimeout, clearTimeout, setInterval, clearInterval
// <https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout>
if (!("setTimeout" in this)) {
  // Not-very-compliant polyfill for setTimeout, based on Promise.

  // When this array becomes non-empty, call scheduleWork.
  let timeouts = [];

  // Fake clock used to order timeouts.
  let now = 0;

  // Least positive integer not yet used as a timer id.
  let next_id = 1;

  // Add an active timer record to timeouts.
  function enqueue(id, t, cb) {
    if (typeof cb !== 'function') {
      let code = "" + cb;
      cb = () => eval(code);
    }
    t += now;
    timeouts.push({id, t, cb});
    timeouts.sort((a, b) => a.t - b.t);
    if (timeouts.length === 1) {
      scheduleWork();
    }
  }

  // Register a microtask to make sure timers are actually processed. There
  // should be at most one of these microtasks pending at a time.
  function scheduleWork() {
    // Number of times to let the microtask queue spin before running a
    // timeout. The idea is to let all Promises resolve between consecutive
    // timer callbacks. Of course, this is a hack; whatever number is used
    // here, an async function can simply await that many times plus one to
    // observe that timeouts are being handled as microtasks, not events.
    let delay = 10;

    function handleTimeouts() {
      if (--delay > 0) {
        Promise.resolve().then(handleTimeouts);
        return;
      }
      if (timeouts.length > 0) {
        let {t, cb} = timeouts.shift();
        if (now < t) {
          now = t;
        }
        if (timeouts.length > 0) {
          scheduleWork();  // There's more to do after this.
        }
        cb();
      }
    }
    Promise.resolve().then(handleTimeouts);
  }

  this.setTimeout = function (cb, t, ...args) {
    let id = next_id++;
    enqueue(id, t, () => cb(...args));
    return id;
  };

  this.clearTimeout = function (id) {
    timeouts = timeouts.filter(obj => obj.id !== id);
  };

  this.setInterval = function (cb, t, ...args) {
    let id = next_id++;
    let func = () => {
      enqueue(id, t, func);
      cb(...args);
    };
    enqueue(id, t, func);
    return id;
  };

  this.clearInterval = this.clearTimeout;
}

// Some tests in web platform tests leave promise rejections unhandled.
if ("ignoreUnhandledRejections" in this) {
  ignoreUnhandledRejections();
}
