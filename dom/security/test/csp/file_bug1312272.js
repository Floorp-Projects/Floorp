var m = document.getElementById("m");
m.addEventListener("click", function() {
  // this will trigger after onstart, obviously.
  parent.postMessage('finish', '*');
});
console.log("finish-handler setup");
m.click();
console.log("clicked");
