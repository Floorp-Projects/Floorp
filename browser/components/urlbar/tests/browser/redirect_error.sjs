/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const REDIRECT_TO = "https://www.bank1.com/"; // Bad-cert host.

function handleRequest(aRequest, aResponse) {
 // Set HTTP Status
 aResponse.setStatusLine(aRequest.httpVersion, 301, "Moved Permanently");

 // Set redirect URI, mirroring the hash value.
 let hash = (/\#.+/.test(aRequest.path))? 
              "#" + aRequest.path.split("#")[1]:
              "";
 aResponse.setHeader("Location", REDIRECT_TO + hash);
}
