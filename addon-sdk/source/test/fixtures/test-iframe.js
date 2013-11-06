
var count = 0;

setTimeout(function() {
  window.addEventListener("message", function(msg) {
    if (++count > 1) {
    	self.postMessage(msg.data);
    }
    else msg.source.postMessage(msg.data, '*');
  });

  document.getElementById('inner').src = iframePath;
}, 0);
