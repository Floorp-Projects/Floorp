// this will take strings_to_send.length*500 ms = 5 sec

var timer = null;
var strings_to_send = ["data\r\n\nda", "ta", ":", "de", "layed1\n\n",
                       "",
                       "",
                       "data:delayed2\n\n", "", ""];
var resp = null;

function sendNextString()
{
  if (strings_to_send.length == 0) {
    timer.cancel();
    resp.finish();
    timer = null;
    resp = null;
    return;
  }

  resp.write(strings_to_send[0]);
  strings_to_send = strings_to_send.splice(1, strings_to_send.length - 1);
}

function handleRequest(request, response)
{
  var b = 0;
  for (var i=0; i < strings_to_send.length; ++i) {
    b += strings_to_send[i].length;
  }
  
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n")
  response.write("Content-Length: " + b + "\r\n");
  response.write("Content-Type: text/event-stream; charset=utf-8\r\n");
  response.write("Cache-Control: no-cache, must-revalidate\r\n");
  response.write("\r\n");
  
  resp = response;

  timer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(sendNextString, 500, Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
}
