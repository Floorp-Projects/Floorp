var xhr = new XMLHttpRequest();
xhr.open("POST", "worker_testXHR.txt");
xhr.send();
try {
  xhr.send();
  postMessage("KO double send should fail");
} catch (e) {
  postMessage(
    e.name === "InvalidStateError" ? "OK" : "KO InvalidStateError expected"
  );
}
