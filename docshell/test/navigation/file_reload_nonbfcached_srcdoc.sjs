const createPage = function (msg) {
  return `
<html>
<script>
  onpageshow = function() {
    opener.postMessage(document.body.firstChild.contentDocument.body.textContent);
  }
</script>
<body><iframe srcdoc="${msg}"></iframe><body>
</html>
`;
};

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "text/html");

  let currentState = getState("reload_nonbfcached_srcdoc");
  let srcdoc = "pageload:" + currentState;
  if (currentState != "second") {
    setState("reload_nonbfcached_srcdoc", "second");
  } else {
    setState("reload_nonbfcached_srcdoc", "");
  }

  response.write(createPage(srcdoc));
}
