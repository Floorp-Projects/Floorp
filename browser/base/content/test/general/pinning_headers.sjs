const INVALIDPIN1 = "pin-sha256=\"d6qzRu9zOECb90Uez27xWltNsj0e1Md7GkYYkVoZWmM=\";";
const INVALIDPIN2 = "pin-sha256=\"AAAAAAAAAAAAAAAAAAAAAAAAAj0e1Md7GkYYkVoZWmM=\";";
const VALIDPIN = "pin-sha256=\"hXweb81C3HnmM2Ai1dnUzFba40UJMhuu8qZmvN/6WWc=\";";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  switch (request.queryString) {
    case "zeromaxagevalid":
      response.setHeader("Public-Key-Pins", "max-age=0;" + VALIDPIN +
                                            INVALIDPIN2 + "includeSubdomains");
      break;
    case "valid":
    default:
      response.setHeader("Public-Key-Pins", "max-age=50000;" + VALIDPIN +
                                            INVALIDPIN2 + "includeSubdomains");
  }

  response.write("Hello world!" + request.host);
}
