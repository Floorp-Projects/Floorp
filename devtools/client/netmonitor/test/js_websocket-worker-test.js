"use strict";
openWorkerSocket();

function openWorkerSocket() {
  this.ws = new WebSocket("wss://localhost:8081");
}
