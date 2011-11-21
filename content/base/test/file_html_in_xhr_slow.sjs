var timer;

function handleRequest(request, response)
{
  var converter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
  converter.charset = "windows-1251";
  var stream = converter.convertToInputStream("\u042E");
  var out = response.bodyOutputStream;
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  out.writeFrom(stream, 1);
  var firstPart = "<meta charset='windows";
  out.write(firstPart, firstPart.length);
  out.flush();
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.write("-1251'>");      
      response.finish();
    }, 500, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

