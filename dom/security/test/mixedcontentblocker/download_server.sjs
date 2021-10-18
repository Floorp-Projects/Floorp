// force the Browser to Show a Download Prompt

function handleRequest(request, response) {
  let type = "image/png";
  let filename = "hello.png";
  request.queryString.split("&").forEach(val => {
    var [key, value] = val.split("=");
    if (key == "type") {
      type = value;
    }
    if (key == "name") {
      filename = value;
    }
  });

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Disposition", `attachment; filename=${filename}`);
  response.setHeader("Content-Type", type);
  response.write("🙈🙊🐵🙊");
}
