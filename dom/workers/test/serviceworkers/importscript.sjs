function handleRequest(request, response) {
  if (!getState('counter')) {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("callByScript();");
    setState('counter', '1');
  } else {
    response.write("no cache no party!");
  }
}
