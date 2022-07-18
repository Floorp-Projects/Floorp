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
    add_task.index = 0;
  }

  add_task.tasks.push(func);
}

addEventListener("message", async function onMessage(event) {
  function info(message) {
    postMessage({ op: "info", message });
  }

  function executeSoon(callback) {
    const channel = new MessageChannel();
    channel.port1.postMessage("");
    channel.port2.onmessage = function() {
      callback();
    };
  }

  function runNextTest() {
    if (add_task.index < add_task.tasks.length) {
      const task = add_task.tasks[add_task.index++];
      info("add_task | Entering test " + task.name);
      task()
        .then(function() {
          executeSoon(runNextTest);
          info("add_task | Leaving test " + task.name);
        })
        .catch(function(ex) {
          postMessage({ op: "failure", message: "" + ex });
        });
    } else {
      postMessage({ op: "finish" });
    }
  }

  removeEventListener("message", onMessage);

  const data = event.data;
  importScripts(...data);

  executeSoon(runNextTest);
});
