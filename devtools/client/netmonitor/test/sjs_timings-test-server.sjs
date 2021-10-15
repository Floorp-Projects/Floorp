/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var trailerServerTiming = [
  {metric:"metric3", duration:"99789.11", description:"time3"},
  {metric:"metric4", duration:"1112.13", description:"time4"}
];

var responseServerTiming = [
  {metric:"metric1", duration:"123.4", description:"time1"},
  {metric:"metric2", duration:"0", description:"time2"}
];

function handleRequest(request, response) {
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

function createServerTimingHeader(headerData) {
  var header = "";
  for (var i = 0; i < headerData.length; i++) {
    header += "Server-Timing: " + headerData[i].metric + ";" +
      "dur=" + headerData[i].duration + ";" +
      "desc=" + headerData[i].description + "\r\n";
  }
  return header;
}
