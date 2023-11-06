"use strict";

console.log("script evaluation");
console.log("Here is a SAB", new SharedArrayBuffer(1024));

addEventListener("install", function (evt) {
  console.log("install event");
});

addEventListener("activate", function (evt) {
  console.log("activate event");
});

addEventListener("fetch", function (evt) {
  console.log("fetch event: " + evt.request.url);
  evt.respondWith(new Response("Hello world"));
});

addEventListener("message", function (evt) {
  console.log("message event: " + evt.data.message);
  evt.source.postMessage({ type: "PONG" });
});
