var OMIT = {};
var WRITABLES = [true, false, OMIT];
{
  var desc = {};
  function put(field, value) {
    return desc[field] = value;
  }
  WRITABLES.forEach(function(writable) {
    put("writable", writable)
  });
};
