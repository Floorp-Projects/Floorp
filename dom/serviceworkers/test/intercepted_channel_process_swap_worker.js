onfetch = e => {
  const url = new URL(e.request.url).searchParams.get("respondWith");

  if (url) {
    e.respondWith(fetch(url));
  }
};
