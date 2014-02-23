 addEventListener("error", function(e) {
     var obj = {};
     for (var prop of ["message", "filename", "lineno"]) {
       obj[prop] = e[prop]
     }
     obj.type = "event";
     postMessage(obj);
});
onerror = function(message, filename, lineno) {
  var obj = { message: message, filename: filename, lineno: lineno,
	      type: "callback" }
  postMessage(obj);
  return false;
}
throw new Error("workerhello");
