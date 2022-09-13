/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Access-Control-Allow-Credentials", "true");
  response.setHeader("Content-Type", "application/json");
  let content = {
    accounts: [
      {
        id: "1234",
        given_name: "John",
        name: "John Doe",
        email: "john_doe@idp.example",
        picture: "https://idp.example/profile/123",
        approved_clients: ["123", "456", "789"],
      },
    ],
  };
  let body = JSON.stringify(content);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(body);
}
