function wrapNode() {
  var elm = document.getElementById("test");
  var span = document.createElement("span");
  span.setAttribute("style", "background: yellow");
  var range = document.createRange();
  var start = "first second third [".length;
  range.setStart(elm.lastChild, start);
  range.setEnd(elm.lastChild, start + "fourth".length);
  range.surroundContents(span);
}

window.addEventListener("load", wrapNode, false);
