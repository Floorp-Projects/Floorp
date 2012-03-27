var count = 0;
function timerFunction() {
  if (++count == 30) {
    close();
    postMessage("ready");
    while (true) { }
  }
}

for (var i = 0; i < 10; i++) {
  setInterval(timerFunction, 500);
}
