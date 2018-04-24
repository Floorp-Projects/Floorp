
var responseServerTiming = [{metric:"metric1", duration:"123.4", description:"description1"},
                           {metric:"metric2", duration:"456.78", description:"description2"}];
var trailerServerTiming = [{metric:"metric3", duration:"789.11", description:"description3"},
                           {metric:"metric4", duration:"1112.13", description:"description4"}];

function createServerTimingHeader(headerData) {
  var header = "";
  for (var i = 0; i < headerData.length; i++) {
    header += "Server-Timing:" + headerData[i].metric + ";" +
              "dur=" + headerData[i].duration + ";" +
              "desc=" + headerData[i].description + "\r\n";
  }
  return header;
}

function handleRequest(request, response)
{
  var body = "c\r\ndata reached\r\n3\r\nhej\r\n0\r\n";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write(createServerTimingHeader(responseServerTiming));

  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.write(createServerTimingHeader(trailerServerTiming));
  response.write("\r\n");
  response.finish();
}
