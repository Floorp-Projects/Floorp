"use strict";

const HTML_DATA = `
  <!DOCTYPE HTML>
  <html>
  <head>
    <meta charset="utf-8">
    <title>Test for Bug 1222285</title>
    <script type="application/javascript">
      function populateKeyEventResult(aEvent) {
        let result = document.getElementById("result-" + aEvent.type);
        let data = {
          key: aEvent.key,
          code: aEvent.code,
          location: aEvent.location,
          altKey: aEvent.altKey,
          shiftKey: aEvent.shiftKey,
          ctrlKey: aEvent.ctrlKey,
          charCode: aEvent.charCode,
          keyCode: aEvent.keyCode,
          modifierState: {
            Alt: aEvent.getModifierState("Alt"),
            AltGraph: aEvent.getModifierState("AltGraph"),
            Shift: aEvent.getModifierState("Shift"),
            Control: aEvent.getModifierState("Control")
          }
        };

        result.value = JSON.stringify(data);
        result.dispatchEvent(new CustomEvent("resultAvailable"));
      }

      window.onload = () => {
        for (event of ["keydown", "keypress", "keyup"]) {
          document.getElementById("test")
                  .addEventListener(event, populateKeyEventResult);
        }
      }
    </script>
  </head>
  <body>
    <input id="test"/>
    <input id="result-keydown"/>
    <input id="result-keypress"/>
    <input id="result-keyup"/>
  </body>
  </html>`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  let queryString = new URLSearchParams(request.queryString);

  let language = queryString.get("language");

  if (language) {
    response.setHeader("Content-Language", language, false);
  }

  response.write(HTML_DATA);
}
