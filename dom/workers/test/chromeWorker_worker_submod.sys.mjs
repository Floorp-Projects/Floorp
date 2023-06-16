onmessage = function (event) {
  let worker = new ChromeWorker("chromeWorker_subworker.js");
  worker.onmessage = function (msg) {
    postMessage(msg.data);
  };
  worker.postMessage(event.data);
};

export default "go away linter";
