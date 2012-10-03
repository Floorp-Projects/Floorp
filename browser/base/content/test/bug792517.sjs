/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(aRequest, aResponse) {
 aResponse.setStatusLine(aRequest.httpVersion, 200);
 if (aRequest.hasHeader('Cookie')) {
   aResponse.write("cookie-present");
 } else {
   aResponse.setHeader("Set-Cookie", "foopy=1");
   aResponse.write("cookie-not-present");
 }
}
