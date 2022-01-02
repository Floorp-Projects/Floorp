navigator.locks.request("exclusive", () => {
  const channel = new BroadcastChannel("strongworker");
  channel.postMessage("lock acquired");
});
postMessage("onload");
