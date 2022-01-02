// Add the correct border/margin/padding style
if (window.location.search.length > 0) {
  var params = window.location.search.substr(1).split("_");
  if (params[0] == "border") {
    params[0] = "border-width";
  }
  document.write("<style>");
  document.write((params[1] == "parent") ? "#parent" : "#child");
  document.write("{ " + params[0] + ": 1px 2px 3px 4px; }</style>");
}
