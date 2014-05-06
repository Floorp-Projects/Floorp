onmessage = function(evt) {
  console.time('bug1004814');
  setTimeout(function() {
    console.timeEnd('bug1004814');
  }, 200);
}
