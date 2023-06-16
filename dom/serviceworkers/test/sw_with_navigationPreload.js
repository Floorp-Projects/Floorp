addEventListener("activate", event => {
  event.waitUntil(self.registration.navigationPreload.enable());
});

async function post_to_page(data) {
  let cs = await self.clients.matchAll();
  for (const client of cs) {
    client.postMessage(data);
  }
}

addEventListener("fetch", event => {
  if (event.request.url.includes("navigationPreload_page.html")) {
    event.respondWith(
      new Response("<!DOCTYPE html>", {
        headers: { "Content-Type": "text/html; charset=utf-8" },
      })
    );

    event.waitUntil(
      (async function () {
        let preloadResponse = await event.preloadResponse;
        let text = await preloadResponse.text();
        await post_to_page(text);
      })()
    );
  }
});
