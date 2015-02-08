var gBasePath = "tests/dom/apps/tests/";
var gAppTemplatePath = "tests/dom/apps/tests/file_app.template.html";
var gScriptTemplatePath = "tests/dom/apps/tests/file_script.template.js";
var gAppcacheTemplatePath = "tests/dom/apps/tests/file_cached_app.template.appcache";
var gWidgetTemplatePath = "tests/dom/apps/tests/file_widget_app.template.html";
var gDefaultIcon = "default_icon";

function makeResource(templatePath, version, apptype, role) {
  let icon = getState('icon') || gDefaultIcon;
  var res = readTemplate(templatePath).replace(/VERSIONTOKEN/g, version)
                                      .replace(/APPTYPETOKEN/g, apptype)
                                      .replace(/ICONTOKEN/g, icon)
                                      .replace(/ROLE/g, role);

  // Hack - This is necessary to make the tests pass, but hbambas says it
  // shouldn't be necessary. Comment it out and watch the tests fail.
  if (templatePath == gAppTemplatePath &&
      (apptype == 'cached' || apptype == 'trusted')) {
    res = res.replace('<html>', '<html manifest="file_app.sjs?apptype=' + apptype + '&getappcache=true">');
  }
  return res;
}

function handleRequest(request, response) {
  var query = getQuery(request);

  // If this is a version update, update state and return.
  if ("setVersion" in query) {
    setState('version', query.setVersion);
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.write('OK');
    return;
  }

  if ("setIcon" in query) {
    let icon = query.setIcon;
    if (icon === 'DEFAULT') {
      icon = null;
    }

    setState('icon', icon);

    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.write('OK');
    return;
  }

  // Get the app type.
  var apptype = query.apptype;
  if (apptype != 'hosted' && apptype != 'cached' && apptype != 'widget'  && apptype != 'invalidWidget' && apptype != 'trusted')
    throw "Invalid app type: " + apptype;

  var role = query.role;

  // Get the version from server state and handle the etag.
  var version = Number(getState('version'));
  var etag = getEtag(request, version);
  dump("Server Etag: " + etag + "\n");

  if (etagMatches(request, etag)) {
    dump("Etags Match. Sending 304 for " + request.queryString + "\n");
    response.setStatusLine(request.httpVersion, "304", "Not Modified");
    return;
  }

  response.setHeader("Etag", etag, false);
  if (request.hasHeader("If-None-Match"))
    dump("Client Etag: " + request.getHeader("If-None-Match") + "\n");
  else
    dump("No Client Etag\n");

  // Check if we ask for the js script used in file_app.template.html
  if ("script" in query) {
   dump("Sending script for " + request.queryString + "\n");
   response.write(makeResource(gScriptTemplatePath, version, apptype, role));
   return;
  }

  // Check if we're generating a webapp manifest.
  if ('getmanifest' in query) {
    var template = gBasePath + 'file_' + apptype + '_app.template.webapp';
    response.setHeader("Content-Type", "application/x-web-app-manifest+json", false);
    response.write(makeResource(template, version, apptype, role));
    return;
  }

  // If apptype==cached, we might be generating the appcache manifest.
  //
  // NB: Among other reasons, we use the same sjs file here so that the version
  //     state is shared.
  if ((apptype == 'cached' || apptype == 'trusted') &&
      'getappcache' in query) {
    response.setHeader("Content-Type", "text/cache-manifest", false);
    response.write(makeResource(gAppcacheTemplatePath, version, apptype, role));
    return;
  }
  else if (apptype == 'widget' || apptype == 'invalidWidget')
  {
    response.setHeader("Content-Type", "text/html", false);
    response.write(makeResource(gWidgetTemplatePath, version, apptype, role));
    return;
  }
  // Generate the app.
  response.setHeader("Content-Type", "text/html", false);
  response.write(makeResource(gAppTemplatePath, version, apptype, role));
}

function getEtag(request, version) {
  return request.queryString.replace(/&/g, '-').replace(/=/g, '-') + '-' + version;
}

function etagMatches(request, etag) {
  return request.hasHeader("If-None-Match") && request.getHeader("If-None-Match") == etag;
}

function getQuery(request) {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  return query;
}

// Copy-pasted incantations. There ought to be a better way to synchronously read
// a file into a string, but I guess we're trying to discourage that.
function readTemplate(path) {
  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var cis = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
                       createInstance(Components.interfaces.nsIConverterInputStream);
  var split = path.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  cis.init(fis, "UTF-8", 0, 0);

  var data = "";
  let str = {};
  let read = 0;
  do {
    read = cis.readString(0xffffffff, str); // read as much as we can and put it in str.value
    data += str.value;
  } while (read != 0);
  cis.close();
  return data;
}
