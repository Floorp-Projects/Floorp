/*
 * Description of the tests:
 *   Check that HSTS priming occurs correctly with mixed content
 *
 *   This test uses three hostnames, each of which treats an HSTS priming
 *   request differently.
 *   * no-ssl never returns an ssl response
 *   * reject-upgrade returns an ssl response, but with no STS header
 *   * prime-hsts returns an ssl response with the appropriate STS header
 *
 *   For each server, test that it response appropriately when the we allow
 *   or block active or display content, as well as when we send an hsts priming
 *   request, but do not change the order of mixed-content and HSTS.
 *
 *   Test use http-on-examine-response, so must be run in browser context.
 */
'use strict';

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var TOP_URI = "https://example.com/browser/dom/security/test/hsts/file_priming-top.html";

var test_servers = {
  // a test server that does not support TLS
  'no-ssl': {
    host: 'example.co.jp',
    response: false,
    id: 'no-ssl',
  },
  // a test server which does not support STS upgrade
  'reject-upgrade': {
    host: 'example.org',
    response: true,
    id: 'reject-upgrade',
  },
  // a test server when sends an STS header when priming
  'prime-hsts': {
    host: 'test1.example.com',
    response: true,
    id: 'prime-hsts'
  },
};

var test_settings = {
  // mixed active content is allowed, priming will upgrade
  allow_active: {
    block_active: false,
    block_display: false,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'script',
    timeout: 0,
    result: {
      'no-ssl': 'insecure',
      'reject-upgrade': 'insecure',
      'prime-hsts': 'secure',
    },
  },
  // mixed active content is blocked, priming will upgrade
  block_active: {
    block_active: true,
    block_display: false,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'script',
    timeout: 0,
    result: {
      'no-ssl': 'blocked',
      'reject-upgrade': 'blocked',
      'prime-hsts': 'secure',
    },
  },
  // keep the original order of mixed-content and HSTS, but send
  // priming requests
  hsts_after_mixed: {
    block_active: true,
    block_display: false,
    use_hsts: false,
    send_hsts_priming: true,
    type: 'script',
    timeout: 0,
    result: {
      'no-ssl': 'blocked',
      'reject-upgrade': 'blocked',
      'prime-hsts': 'blocked',
    },
  },
  // mixed display content is allowed, priming will upgrade
  allow_display: {
    block_active: true,
    block_display: false,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'img',
    timeout: 0,
    result: {
      'no-ssl': 'insecure',
      'reject-upgrade': 'insecure',
      'prime-hsts': 'secure',
    },
  },
  // mixed display content is blocked, priming will upgrade
  block_display: {
    block_active: true,
    block_display: true,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'img',
    timeout: 0,
    result: {
      'no-ssl': 'blocked',
      'reject-upgrade': 'blocked',
      'prime-hsts': 'secure',
    },
  },
  // mixed active content is blocked, priming will upgrade (css)
  block_active_css: {
    block_active: true,
    block_display: true,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'css',
    timeout: 0,
    result: {
      'no-ssl': 'blocked',
      'reject-upgrade': 'blocked',
      'prime-hsts': 'secure',
    },
  },
  // mixed active content is blocked, priming will upgrade
  // redirect to the same host
  block_active_with_redir_same: {
    block_active: true,
    block_display: false,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'script',
    redir: 'same',
    timeout: 0,
    result: {
      'no-ssl': 'blocked',
      'reject-upgrade': 'blocked',
      'prime-hsts': 'secure',
    },
  },
  // mixed active content is blocked, priming will upgrade
  // redirect to the same host
  timeout: {
    block_active: true,
    block_display: true,
    use_hsts: true,
    send_hsts_priming: true,
    type: 'script',
    timeout: 100000,
    result: {
      'no-ssl': 'blocked',
      'reject-upgrade': 'blocked',
      'prime-hsts': 'blocked',
    },
  },
}
// track which test we are on
var which_test = "";

/**
 * A stream listener that just forwards all calls
 */
var StreamListener = function(subject) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  let traceable = subject.QueryInterface(Ci.nsITraceableChannel);

  this.uri = channel.URI.asciiSpec;
  this.listener = traceable.setNewListener(this);
  return this;
};

// Next three methods are part of `nsIStreamListener` interface and are
// invoked by `nsIInputStreamPump.asyncRead`.
StreamListener.prototype.onDataAvailable = function(request, context, input, offset, count) {
  if (request.status == Cr.NS_ERROR_ABORT) {
    this.listener = null;
    return Cr.NS_SUCCESS;
  }
  let listener = this.listener;
  if (listener) {
    try {
      let rv = listener.onDataAvailable(request, context, input, offset, count);
      if (rv != Cr.NS_ERROR_ABORT) {
        // If the channel gets canceled, we sometimes get NS_ERROR_ABORT here.
        // Anything else is an error.
        return rv;
      }
    } catch (e) {
      if (e != Cr.NS_ERROR_ABORT) {
        return e;
      }
    }
  }
  return Cr.NS_SUCCESS;
};

// Next two methods implement `nsIRequestObserver` interface and are invoked
// by `nsIInputStreamPump.asyncRead`.
StreamListener.prototype.onStartRequest = function(request, context) {
  if (request.status == Cr.NS_ERROR_ABORT) {
    this.listener = null;
    return Cr.NS_SUCCESS;
  }
  let listener = this.listener;
  if (listener) {
    try {
      let rv = listener.onStartRequest(request, context);
      if (rv != Cr.NS_ERROR_ABORT) {
        // If the channel gets canceled, we sometimes get NS_ERROR_ABORT here.
        // Anything else is an error.
        return rv;
      }
    } catch (e) {
      if (e != Cr.NS_ERROR_ABORT) {
        return e;
      }
    }
  }
  return Cr.NS_SUCCESS;
};

// Called to signify the end of an asynchronous request. We only care to
// discover errors.
StreamListener.prototype.onStopRequest = function(request, context, status) {
  if (status == Cr.NS_ERROR_ABORT) {
    this.listener = null;
    return Cr.NS_SUCCESS;
  }
  let listener = this.listener;
  if (listener) {
    try {
      let rv = listener.onStopRequest(request, context, status);
      if (rv != Cr.NS_ERROR_ABORT) {
        // If the channel gets canceled, we sometimes get NS_ERROR_ABORT here.
        // Anything else is an error.
        return rv;
      }
    } catch (e) {
      if (e != Cr.NS_ERROR_ABORT) {
        return e;
      }
    }
  }
  return Cr.NS_SUCCESS;
};

var Observer = {
  listeners: {},
  observe: function (subject, topic, data) {
    switch (topic) {
      case 'console-api-log-event':
        return Observer.console_api_log_event(subject, topic, data);
      case 'http-on-examine-response':
        return Observer.http_on_examine_response(subject, topic, data);
      case 'http-on-modify-request':
        return Observer.http_on_modify_request(subject, topic, data);
    }
    throw "Can't handle topic "+topic;
  },
  add_observers: function (services, include_on_modify = false) {
    services.obs.addObserver(Observer, "console-api-log-event", false);
    services.obs.addObserver(Observer, "http-on-examine-response", false);
    services.obs.addObserver(Observer, "http-on-modify-request", false);
  },
  cleanup: function () {
    this.listeners = {};
  },
  // When a load is blocked which results in an error event within a page, the
  // test logs to the console.
  console_api_log_event: function (subject, topic, data) {
    var message = subject.wrappedJSObject.arguments[0];
    // when we are blocked, this will match the message we sent to the console,
    // ignore everything else.
    var re = RegExp(/^HSTS_PRIMING: Blocked ([-\w]+).*$/);
    if (!re.test(message)) {
      return;
    }

    let id = message.replace(re, '$1');
    let curTest =test_servers[id];

    if (!curTest) {
      ok(false, "HSTS priming got a console message blocked, "+
          "but doesn't match expectations "+id+" (msg="+message);
      return;
    }

    is("blocked", test_settings[which_test].result[curTest.id], "HSTS priming "+
        which_test+":"+curTest.id+" expected "+
        test_settings[which_test].result[curTest.id]+", got blocked");
    test_settings[which_test].finished[curTest.id] = "blocked";
  },
  get_current_test: function(uri) {
    for (let item in test_servers) {
      let re = RegExp('https?://'+test_servers[item].host+'.*\/browser/dom/security/test/hsts/file_testserver.sjs');
      if (re.test(uri)) {
        return test_servers[item];
      }
    }
    return null;
  },
  http_on_modify_request: function (subject, topic, data) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    let uri = channel.URI.asciiSpec;

    let curTest = this.get_current_test(channel.URI.asciiSpec);

    if (!curTest) {
      return;
    }
    
    if (!(uri in this.listeners)) {
      // Add an nsIStreamListener to ensure that the listener is not NULL
      this.listeners[uri] = new StreamListener(subject);
    }

    if (channel.requestMethod != 'HEAD') {
      return;
    }
    if (typeof ok === 'undefined') {
      // we are in the wrong thread and ok and is not available
      return;
    }
    ok(!(curTest.id in test_settings[which_test].priming), "Already saw a priming request for " + curTest.id);
    test_settings[which_test].priming[curTest.id] = true;
  },
  // When we see a response come back, peek at the response and test it is secure
  // or insecure as needed. Addtionally, watch the response for priming requests.
  http_on_examine_response: function (subject, topic, data) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    let curTest = this.get_current_test(channel.URI.asciiSpec);
    let uri = channel.URI.asciiSpec;

    if (!curTest) {
      return;
    }

    let result = (channel.URI.asciiSpec.startsWith('https:')) ? "secure" : "insecure";

    // This is a priming request, go ahead and validate we were supposed to see
    // a response from the server
    if (channel.requestMethod == 'HEAD') {
      is(true, curTest.response, "HSTS priming response found " + curTest.id);
      return;
    }

    // This is the response to our query, make sure it matches
    is(result, test_settings[which_test].result[curTest.id],
        "HSTS priming result " + which_test + ":" + curTest.id);
    test_settings[which_test].finished[curTest.id] = result;
    if (this.listeners[uri]) {
      this.listeners[uri] = undefined;
    }
  },
};

// opens `uri' in a new tab and focuses it.
// returns the newly opened tab
function openTab(uri) {
  let tab = gBrowser.addTab(uri);

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  return tab;
}

function clear_sts_data() {
  for (let test in test_servers) {
    SpecialPowers.cleanUpSTSData('http://'+test_servers[test].host);
  }
}

function do_cleanup() {
  clear_sts_data();

  Services.obs.removeObserver(Observer, "console-api-log-event");
  Services.obs.removeObserver(Observer, "http-on-examine-response");

  Observer.cleanup();
}

function SetupPrefTestEnvironment(which, additional_prefs) {
  which_test = which;
  clear_sts_data();

  var settings = test_settings[which];
  // priming counts how many priming requests we saw
  settings.priming = {};
  // priming counts how many tests were finished
  settings.finished= {};

  var prefs = [["security.mixed_content.block_active_content",
                settings.block_active],
               ["security.mixed_content.block_display_content",
                settings.block_display],
               ["security.mixed_content.use_hsts",
                settings.use_hsts],
               ["security.mixed_content.send_hsts_priming",
                settings.send_hsts_priming],
  ];

  if (additional_prefs) {
    for (let idx in additional_prefs) {
      prefs.push(additional_prefs[idx]);
    }
  }

  SpecialPowers.pushPrefEnv({'set': prefs});
}

// make the top-level test uri
function build_test_uri(base_uri, host, test_id, type, timeout) {
  return base_uri +
          "?host=" + escape(host) +
          "&id=" + escape(test_id) +
          "&type=" + escape(type) +
          "&timeout=" + escape(timeout)
    ;
}

// open a new tab, load the test, and wait for it to finish
async function execute_test(test, mimetype) {
  var src = build_test_uri(TOP_URI, test_servers[test].host,
      test, test_settings[which_test].type,
      test_settings[which_test].timeout);

  await BrowserTestUtils.withNewTab(src, () => {});
}
