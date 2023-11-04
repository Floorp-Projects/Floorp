function handleRequest(request, response) {
  if (request.queryString == "reset") {
    setState("index", "0");
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.write("Reset");
    return;
  }

  let refresh = "";
  let index = Number(getState("index"));
  // index == 0 First load, returns first meta refresh
  // index == 1 Second load, caused by first meta refresh, returns second meta refresh
  // index == 2 Third load, caused by second meta refresh, doesn't return a meta refresh
  let query = new URLSearchParams(request.queryString);
  if (index < 2) {
    refresh = query.get("seconds");
    if (query.get("crossOrigin") == "true") {
      const hosts = ["example.org", "example.com"];

      let url = `${request.scheme}://${hosts[index]}${request.path}?${request.queryString}`;
      refresh += `; url=${url}`;
    }
    refresh = `<meta http-equiv="Refresh" content="${refresh}">`;
  }
  // We want to scroll for the first load, and check that the meta refreshes keep the same
  // scroll position.
  let scroll = index == 0 ? `scrollTo(0, ${query.get("scrollTo")});` : "";

  setState("index", String(index + 1));

  response.write(
    `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="Cache-Control" content="no-cache">
  ${refresh}
  <script>
    window.addEventListener("pageshow", () => {
      ${scroll}
      window.top.opener.postMessage({
        commandType: "pageShow",
        commandData: {
          inputValue: document.getElementById("input").value,
          scrollPosition: window.scrollY,
        },
      }, "*");
    });
    window.addEventListener("message", ({ data }) => {
      if (data == "changeInputValue") {
        document.getElementById("input").value = "1234";
        window.top.opener.postMessage({
          commandType: "onChangedInputValue",
          commandData: {
            historyLength: history.length,
            inputValue: document.getElementById("input").value,
          },
        }, "*");
      } else if (data == "loadNext") {
        location.href += "&loadnext=1";
      } else if (data == "back") {
        history.back();
      }
    });
  </script>
</head>
<body>
<input type="text" id="input" value="initial"></input>
<div style='height: 9000px;'></div>
<p>
</p>
</body>
</html>`
  );
}
