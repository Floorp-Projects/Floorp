function handleRequest(request, response) {
  var match = request.queryString.match(/^state=(.*)$/);
  if (match) {
    response.setStatusLine(request.httpVersion, 204, "No content");
    setState("version", match[1]);
    return;
  }
  const state = getState("version");
  let color = "green";
  if (state === "evil") {
    color = "red";
  }
  const frameContent = `
  <!DOCTYPE html>
  <html>
  <head>
  <style>body,html {background: ${color};}</style>
  </head>
  <body>
  <h1>Offline file: <span id="state">${state}</span></h1>
  `;
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Cache-Control", "no-cache");
  response.write(frameContent);
}
