/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  let issue;
  switch (request.queryString) {
    case "badSyntax":
      response.setHeader("Public-Key-Pins", "\"");
      issue = "is not syntactically correct.";
      break;
    case "noMaxAge":
      response.setHeader("Public-Key-Pins", "max-age444");
      issue = "does not include a max-age directive.";
      break;
    case "invalidIncludeSubDomains":
      response.setHeader("Public-Key-Pins", "includeSubDomains=abc");
      issue = "includes an invalid includeSubDomains directive.";
      break;
    case "invalidMaxAge":
      response.setHeader("Public-Key-Pins", "max-age=abc");
      issue = "includes an invalid max-age directive.";
      break;
    case "multipleIncludeSubDomains":
      response.setHeader("Public-Key-Pins",
                         "includeSubDomains; includeSubDomains");
      issue = "includes multiple includeSubDomains directives.";
      break;
    case "multipleMaxAge":
      response.setHeader("Public-Key-Pins",
                         "max-age=444; max-age=999");
      issue = "includes multiple max-age directives.";
      break;
    case "multipleReportURIs":
      response.setHeader("Public-Key-Pins",
                         'report-uri="http://example.com"; ' +
                         'report-uri="http://example.com"');
      issue = "includes multiple report-uri directives.";
      break;
    case "pinsetDoesNotMatch":
      response.setHeader(
        "Public-Key-Pins",
        'max-age=999; ' +
        'pin-sha256="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="; ' +
        'pin-sha256="BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB="');
      issue = "does not include a matching pin.";
      break;
  }

  response.write("This page is served with a PKP header that " + issue);
}
