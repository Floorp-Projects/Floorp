const es = new EventSource(
  "http://mochi.test:8888/tests/dom/base/test/eventsource_message.sjs"
);
es.onmessage = function() {
  es.close();
};
