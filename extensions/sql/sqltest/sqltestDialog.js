var result;
var enumerator;

function init() {
  result = window.arguments[0];
  enumerator = result.enumerate();
  if (window.arguments.length == 2) {
    enumerator.absolute(window.arguments[1]);
    var columnCount = result.columnCount;
    for(var i = 0; i < columnCount; i++) {
      if (!enumerator.isNull(i)) {
        var element = document.getElementById(result.getColumnName(i));
        element.value = enumerator.getVariant(i);
      }
    }
  }
}

function onAccept() {
  var columnCount = result.columnCount;
  for (var i = 0; i < columnCount; i++) {
    var element = document.getElementById(result.getColumnName(i));
    if (element.value)
      enumerator.setVariant(i, element.value);
    else
      enumerator.setNull(i);
  }

  try {
    if (window.arguments.length == 2)
      enumerator.updateRow();
    else
      enumerator.insertRow();
  }
  catch(ex) {
    alert(enumerator.errorMessage);
    return false;
  }

  return true;
}
