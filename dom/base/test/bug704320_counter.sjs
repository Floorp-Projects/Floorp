// Handle counting loads for bug 704320.

const SHARED_KEY = "bug704320_counter";
const DEFAULT_STATE = {
  css: { count: 0, referrers: [] },
  img: { count: 0, referrers: [] },
  js: { count: 0, referrers: [] },
};
const TYPE_MAP = {
  css: "text/css",
  js: "application/javascript",
  img: "image/png",
  html: "text/html",
};

// Writes an image to the response
function WriteOutImage(response) {
  var file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("CurWorkD", Ci.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");
  file.append("blue.png");

  var fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);
  response.bodyOutputStream.writeFrom(fileStream, fileStream.available());
}

function handleRequest(request, response) {
  var query = {};
  request.queryString.split("&").forEach(function (val) {
    var [name, value] = val.split("=");
    query[name] = unescape(value);
  });

  var referrerLevel = "none";
  if (request.hasHeader("Referer")) {
    let referrer = request.getHeader("Referer");
    if (referrer.indexOf("bug704320") > 0) {
      referrerLevel = "full";
    } else if (referrer == "http://mochi.test:8888/") {
      referrerLevel = "origin";
    }
  }

  var state = getSharedState(SHARED_KEY);
  if (state === "") {
    state = DEFAULT_STATE;
  } else {
    state = JSON.parse(state);
  }

  response.setStatusLine(request.httpVersion, 200, "OK");

  //avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if ("reset" in query) {
    //reset server state
    setSharedState(SHARED_KEY, JSON.stringify(DEFAULT_STATE));
    //serve any CSS that we want to use.
    response.write("");
    return;
  }

  if ("results" in query) {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write(JSON.stringify(state));
    return;
  }

  if ("type" in query) {
    state[query.type].count++;
    response.setHeader("Content-Type", TYPE_MAP[query.type], false);
    if (state[query.type].referrers.indexOf(referrerLevel) < 0) {
      state[query.type].referrers.push(referrerLevel);
    }

    if (query.type == "img") {
      WriteOutImage(response);
    }
  }

  if ("content" in query) {
    response.write(unescape(query.content));
  }

  setSharedState(SHARED_KEY, JSON.stringify(state));
}
