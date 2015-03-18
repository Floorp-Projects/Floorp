var counter = 0;
function handleRequest(request, response) {
  if (!counter) {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("callByScript();");
  } else {
    response.write("no cache no party!");
  }
}
