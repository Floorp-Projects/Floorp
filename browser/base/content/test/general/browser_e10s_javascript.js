const CHROME_PROCESS = Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
const CONTENT_PROCESS = Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

add_task(async function() {
  let url = "javascript:dosomething()";

  ok(E10SUtils.canLoadURIInProcess(url, CHROME_PROCESS),
     "Check URL in chrome process.");
  ok(E10SUtils.canLoadURIInProcess(url, CONTENT_PROCESS),
     "Check URL in content process.");
});
