function send_ping() {
  window.dispatchEvent(new Event("ping"));
}
send_ping(); // ping (=1)

window.addEventListener("load", function () {
  send_ping(); // ping (=2)

  // Append a script which should call |foo|, before the encoding of this script
  // bytecode.
  var script = document.createElement("script");
  script.type = "text/javascript";
  script.innerText = "send_ping();"; // ping (=3)
  document.head.appendChild(script);
});
