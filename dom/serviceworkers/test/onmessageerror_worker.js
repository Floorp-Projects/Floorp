async function getSwContainer() {
  const clients = await self.clients.matchAll({
    type: "window",
    includeUncontrolled: true,
  });

  for (let client of clients) {
    if (client.url.endsWith("test_onmessageerror.html")) {
      return client;
    }
  }
  return undefined;
}

self.addEventListener("message", async e => {
  const config = e.data;
  const swContainer = await getSwContainer();

  if (config == "send-bad-message") {
    const serializable = true;
    const deserializable = false;

    swContainer.postMessage(
      new StructuredCloneTester(serializable, deserializable)
    );

    return;
  }

  if (!config.serializable) {
    swContainer.postMessage({
      result: "Error",
      reason: "Service Worker received an unserializable object",
    });

    return;
  }

  if (!config.deserializable) {
    swContainer.postMessage({
      result: "Error",
      reason:
        "Service Worker received (and deserialized) an un-deserializable object",
    });

    return;
  }

  swContainer.postMessage({ received: "message" });
});

self.addEventListener("messageerror", async () => {
  const swContainer = await getSwContainer();
  swContainer.postMessage({ received: "messageerror" });
});
