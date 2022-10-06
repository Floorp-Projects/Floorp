// This test fails if there is an unhandled promise rejection
var stream = new ReadableStream({
  pull() {
    return Promise.reject("foobar");
  },
});
var response = new Response(stream);
var text = response.text().then(
  () => {},
  e => {}
);
