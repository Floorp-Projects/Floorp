function handleRequest(request, response) {
  response.write(
    `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <script>
    window.addEventListener("message", ({ data }) => {
      if (data == "loadNext") {
        location.href += "&loadnext=1";
        return;
      }
      // Forward other messages to the frame.
      document.getElementById("frame").contentWindow.postMessage(data, "*");
    });
  </script>
</head>
<body>
  <iframe src="file_bug1742865.sjs?${request.queryString}" id="frame"></iframe>
</body>
</html>`
  );
}
