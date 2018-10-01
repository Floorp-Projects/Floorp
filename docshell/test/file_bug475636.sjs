jsURL = "javascript:" + escape('window.parent.postMessage("JS uri ran", "*");\
return \'\
<script>\
window.parent.postMessage("Able to access private: " +\
  window.parent.private, "*");\
</script>\'');
dataURL = "data:text/html," + escape('<!DOCTYPE HTML>\
<script>\
try {\
  window.parent.postMessage("Able to access private: " +\
    window.parent.private, "*");\
}\
catch (e) {\
  window.parent.postMessage("pass", "*");\
}\
</script>');

tests = [
// Plain document should work as normal
'<!DOCTYPE HTML>\
<script>\
try {\
  window.parent.private;\
  window.parent.postMessage("pass", "*");\
}\
catch (e) {\
  window.parent.postMessage("Unble to access private", "*");\
}\
</script>',

// refresh to plain doc
{ refresh: "file_bug475636.sjs?1",
  doc: '<!DOCTYPE HTML>' },

// meta-refresh to plain doc
'<!DOCTYPE HTML>\
<head>\
  <meta http-equiv="refresh" content="0; url=file_bug475636.sjs?1">\
</head>',

// refresh to data url
{ refresh: dataURL,
  doc: '<!DOCTYPE HTML>' },

// meta-refresh to data url
'<!DOCTYPE HTML>\
<head>\
  <meta http-equiv="refresh" content="0; url=' + dataURL + '">\
</head>',

// refresh to js url should not be followed
{ refresh: jsURL,
  doc:
'<!DOCTYPE HTML>\
<script>\
setTimeout(function() {\
  window.parent.postMessage("pass", "*");\
}, 2000);\
</script>' },

// meta refresh to js url should not be followed
'<!DOCTYPE HTML>\
<head>\
  <meta http-equiv="refresh" content="0; url=' + jsURL + '">\
</head>\
<script>\
setTimeout(function() {\
  window.parent.postMessage("pass", "*");\
}, 2000);\
</script>'
];


function handleRequest(request, response)
{
  dump("@@@@@@@@@hi there: " + request.queryString + "\n");
  test = tests[parseInt(request.queryString, 10) - 1];
  response.setHeader("Content-Type", "text/html");

  if (!test) {
    response.write('<script>parent.postMessage("done", "*");</script>');
  }
  else if (typeof test == "string") {
    response.write(test);
  }
  else if (test.refresh) {
    response.setHeader("Refresh", "0; url=" + test.refresh);
    response.write(test.doc);
  }
}
