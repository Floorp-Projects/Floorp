"use strict";

addEventListener("fetch", evt => {
  evt.respondWith(fetch(evt.request));
});
