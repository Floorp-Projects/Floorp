/**
 * Expects a file. Returns an object containing the size, type, name and path.
 */
onmessage = function(event) {
  var file = event.data;

  var rtnObj = new Object();

  rtnObj.size = file.size;
  rtnObj.type = file.type;
  rtnObj.name = file.name;
  rtnObj.path = file.path;
  rtnObj.lastModified = file.lastModified;

  postMessage(rtnObj);
};
