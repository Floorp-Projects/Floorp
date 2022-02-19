"use strict";

const BOUNDARY = "BOUNDARY";

// waitForPageShow should be false if this is for multipart/x-mixed-replace
// and it's not the last part, Gecko doesn't fire pageshow for those parts.
function documentString(waitForPageShow = true) {
  return `<html>
  <head>
    <script>
      let bc = new BroadcastChannel("bug1747033");
      bc.addEventListener("message", ({ data: { cmd, arg = undefined } }) => {
        switch (cmd) {
          case "load":
            location.href = arg;
            break;
          case "replaceState":
            history.replaceState({}, "Replaced state", arg);
            bc.postMessage({ "historyLength": history.length, "location": location.href });
            break;
          case "back":
            history.back();
            break;
          case "close":
            close();
            break;
          }
      });

      function reply() {
        bc.postMessage({ "historyLength": history.length, "location": location.href });
      }

      ${waitForPageShow ? `addEventListener("pageshow", reply);` : "reply();"}
    </script>
  </head>
  <body></body>
</html>
`;
}

function boundary(last = false) {
  let b = `--${BOUNDARY}`;
  if (last) {
    b += "--";
  }
  return b + "\n";
}

function sendMultipart(response, last = false) {
  setState("sendMore", "");

  response.write(`Content-Type: text/html

${documentString(last)}
`);
  response.write(boundary(last));
}

function shouldSendMore() {
  return new Promise(resolve => {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(
      () => {
        let sendMore = getState("sendMore");
        if (sendMore !== "") {
          timer.cancel();
          resolve(sendMore);
        }
      },
      100,
      Ci.nsITimer.TYPE_REPEATING_SLACK
    );
  });
}

async function handleRequest(request, response) {
  if (request.queryString == "") {
    // This is for non-multipart/x-mixed-replace loads.
    response.write(documentString());
    return;
  }

  if (request.queryString == "sendNextPart") {
    setState("sendMore", "next");
    return;
  }

  if (request.queryString == "sendLastPart") {
    setState("sendMore", "last");
    return;
  }

  response.processAsync();

  response.setHeader(
    "Content-Type",
    `multipart/x-mixed-replace; boundary=${BOUNDARY}`,
    false
  );
  response.setStatusLine(request.httpVersion, 200, "OK");

  response.write(boundary());
  sendMultipart(response);
  while ((await shouldSendMore("sendMore")) !== "last") {
    sendMultipart(response);
  }
  sendMultipart(response, true);
  response.finish();
}
