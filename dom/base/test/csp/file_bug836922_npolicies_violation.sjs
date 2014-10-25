// SJS file that receives violation reports and then responds with nothing.

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                              "nsIBinaryInputStream",
                              "setInputStream");

const STATE = "bug836922_violations";

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });


  if ('results' in query) {
    // if asked for the received data, send it.
    response.setHeader("Content-Type", "text/javascript", false);
    if (getState(STATE)) {
      response.write(getState(STATE));
    } else {
      // no state has been recorded.
      response.write(JSON.stringify({}));
    }
  } else if ('reset' in query) {
    //clear state
    setState(STATE, JSON.stringify(null));
  } else {
    // ... otherwise, just respond "ok".
    response.write("null");

    var bodystream = new BinaryInputStream(request.bodyInputStream);
    var avail;
    var bytes = [];
    while ((avail = bodystream.available()) > 0)
      Array.prototype.push.apply(bytes, bodystream.readByteArray(avail));

    var data = String.fromCharCode.apply(null, bytes);

    // figure out which test was violating a policy
    var testpat = new RegExp("testid=([a-z0-9_]+)");
    var testid = testpat.exec(data)[1];

    // store the violation in the persistent state
    var s = getState(STATE);
    if (!s) s = "{}";
    s = JSON.parse(s);
    if (!s) s = {};

    if (!s[testid]) s[testid] = 0;
    s[testid]++;
    setState(STATE, JSON.stringify(s));
  }
}


