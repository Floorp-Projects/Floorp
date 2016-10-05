var count = 0;

function do_magic(data) {
  caches.open("test")
  .then(function(cache) {
    return cache.put("http://mochi.test:888/foo", new Response(data.type + "-" + count++));
  })
  .then(function() {
    if (count == 1) {
      postMessage("ready");
    }

    if (data.loop) {
      setTimeout(function() {do_magic(data); }, 500);
    }
  });
}

onmessage = function(e) {
  if (e.data.type == 'page1') {
    if (e.data.count > 0) {
      var a = new Worker("worker_suspended.js");
      a.postMessage({ type: "page1", count: e.data - 1 });
      a.onmessage = function() { postMessage("ready"); }
    } else {
      do_magic({ type: e.data.type, loop: true });
    }
  } else if (e.data.type == 'page2') {
    do_magic({ type: e.data.type, loop: false });
  }
}
