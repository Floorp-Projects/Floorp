/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Cr = {
  NS_ERROR_NOT_IMPLEMENTED: 2147500033,
};

function add_task(func) {
  if (!add_task.tasks) {
    add_task.tasks = [];
  }

  add_task.tasks.push(func);
}

addEventListener("message", async function onMessage(event) {
  function info(message) {
    postMessage({ op: "info", message });
  }

  removeEventListener("message", onMessage);

  const data = event.data;
  importScripts(...data);

  let task;
  while ((task = add_task.tasks.shift())) {
    info("add_task | Entering test " + task.name);
    await task();
    info("add_task | Leaving test " + task.name);
  }

  postMessage({ op: "finish" });
});
