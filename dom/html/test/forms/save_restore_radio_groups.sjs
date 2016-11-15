var pages = [
  "<!DOCTYPE html>" +
  "<html><body>" +
  "<form>" +
  "<input name='a' type='radio' checked><input name='a' type='radio'><input name='a' type='radio'>" +
  "</form>" +
  "</body></html>",
  "<!DOCTYPE html>" +
  "<html><body>" +
  "<form>" +
  "<input name='a' type='radio'><input name='a' type='radio' checked><input name='a' type='radio'>" +
  "</form>" +
  "</body></html>",
  ];

/**
 * This SJS is going to send the same page the two first times it will be called
 * and another page the two following times. After that, the response will have
 * no content.
 * The use case is to have two iframes using this SJS and both being reloaded
 * once.
 */

function handleRequest(request, response)
{
  var counter = +getState("counter"); // convert to number; +"" === 0

  response.setStatusLine(request.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Cache-Control", "no-cache");

  switch (counter) {
    case 0:
    case 1:
      response.write(pages[0]);
      break;
    case 2:
    case 3:
      response.write(pages[1]);
      break;
  }

  // When we finish the test case we need to reset the counter
  if (counter == 3) {
    setState("counter", "0");
  } else {
    setState("counter", "" + ++counter);
  }
}

