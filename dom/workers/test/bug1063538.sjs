function handleRequest(request, response) {
  response.processAsync();
  response.write("Hello");
  setTimeout(function() { response.finish(); }, 100000); // wait 100 seconds.
}

