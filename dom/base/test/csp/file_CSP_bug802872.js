/*
 *   The policy for this test is:
 *   Content-Security-Policy: default-src 'self'
 */

function createAllowedEvent() {
  /*
   * Creates a new EventSource using 'http://mochi.test:8888'. Since all mochitests run on
   * 'http://mochi.test', a default-src of 'self' allows this request.
   */
  var src_event = new EventSource("http://mochi.test:8888/tests/dom/base/test/csp/file_CSP_bug802872.sjs");

  src_event.onmessage = function(e) {
    src_event.close();
    parent.dispatchEvent(new Event('allowedEventSrcCallbackOK'));
  }

  src_event.onerror = function(e) {
    src_event.close();
    parent.dispatchEvent(new Event('allowedEventSrcCallbackFailed'));
  }
}

function createBlockedEvent() {
  /*
   * creates a new EventSource using 'http://example.com'. This domain is not whitelisted by the 
   * CSP of this page, therefore the CSP blocks this request.
   */
  var src_event = new EventSource("http://example.com/tests/dom/base/test/csp/file_CSP_bug802872.sjs");

  src_event.onmessage = function(e) {
    src_event.close();
    parent.dispatchEvent(new Event('blockedEventSrcCallbackOK'));
  }

  src_event.onerror = function(e) {
    src_event.close();
    parent.dispatchEvent(new Event('blockedEventSrcCallbackFailed'));
  }
}

addLoadEvent(createAllowedEvent);
addLoadEvent(createBlockedEvent);
