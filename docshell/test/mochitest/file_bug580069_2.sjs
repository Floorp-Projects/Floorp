function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/html", false);
  response.write('<html><body onload=\'parent.page2Load("' + request.method + '")\'>file_bug580069_2.sjs</body></html>');
}
