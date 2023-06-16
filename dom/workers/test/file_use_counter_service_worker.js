onmessage = async function (e) {
  if (e.data === "RUN") {
    console.log("worker runs");
    await clients.claim();
    clients.matchAll().then(res => {
      res.forEach(client => client.postMessage("DONE"));
    });
  }
};
