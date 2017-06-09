let timeStampCodes;
let worker = new Worker("worker_grandchild.js");

function listenToParent(event) {
  self.removeEventListener("message", listenToParent);
  timeStampCodes = event.data;

  let timeStamps = [];
  for (let timeStampCode of timeStampCodes) {
    timeStamps.push(eval(timeStampCode));
  }
  // Send the timeStamps to the parent.
  postMessage(timeStamps);

  // Tell the grandchild to start.
  worker.postMessage(timeStampCodes);
}

// The worker grandchild will send results back.
function listenToChild(event) {
  worker.removeEventListener("message", listenToChild);
  // Pass the results to the parent.
  postMessage(event.data);
  worker.terminate();
}

worker.addEventListener("message", listenToChild);
self.addEventListener("message", listenToParent);
