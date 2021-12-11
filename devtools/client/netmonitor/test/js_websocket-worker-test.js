"use strict";
openWorkerSocket();

function openWorkerSocket() {
  new WebSocket("wss://localhost:8081");
}
