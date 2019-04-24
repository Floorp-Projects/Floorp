// This code is evaluated in a sandbox courtesy of toSource();
var sandboxCode = (function() {
  let req = new XMLHttpRequest();
  req.open("GET", "http://mochi.test:8888/browser/dom/tests/browser/", true);
  req.onreadystatechange = function() {
    if (req.readyState === 4) {
      // If we get past the problem above, we end up with a req.status of zero
      // (ie, blocked due to CORS) even though we are fetching from the same
      // origin as the window itself.
      let result;
      if (req.status != 200) {
        result = "ERROR: got request status of " + req.status;
      } else if (req.responseText.length == 0) {
        result = "ERROR: got zero byte response text";
      } else {
        result = "ok";
      }
      postMessage({result}, "*");
    }
  };
  req.send(null);
}).toSource() + "();";

add_task(async function test() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let frame = newWin.document.createElement("iframe");
  frame.setAttribute("type", "content");
  frame.setAttribute("src", "http://mochi.test:8888/browser/dom/tests/browser/browser_xhr_sandbox.js");

  newWin.document.documentElement.appendChild(frame);
  await BrowserTestUtils.waitForEvent(frame, "load", true);

  let contentWindow = frame.contentWindow;
  let sandbox = new Cu.Sandbox(contentWindow);

  // inject some functions from the window into the sandbox.
  // postMessage so the async code in the sandbox can report a result.
  sandbox.importFunction(contentWindow.postMessage.bind(contentWindow), "postMessage");
  sandbox.importFunction(contentWindow.XMLHttpRequest, "XMLHttpRequest");
  Cu.evalInSandbox(sandboxCode, sandbox, "1.8");

  let sandboxReply = await BrowserTestUtils.waitForEvent(contentWindow, "message", true);
  is(sandboxReply.data.result, "ok", "check the sandbox code was felipe");

  await BrowserTestUtils.closeWindow(newWin);
});
