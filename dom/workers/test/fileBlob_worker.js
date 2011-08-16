/**
 * Expects a blob. Returns an object containing the size, type.
 */
onmessage = function(event) {
  var file = event.data;

  var rtnObj = new Object();

  rtnObj.size = file.size;
  rtnObj.type = file.type;

  postMessage(rtnObj);
};
