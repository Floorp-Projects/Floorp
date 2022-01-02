onactivate = e => e.waitUntil(clients.claim());

onfetch = e => {
  const searchParams = new URL(e.request.url).searchParams;

  if (searchParams.get("syntheticResponse") === "1") {
    e.respondWith(new Response(String(searchParams)));
  }
};
