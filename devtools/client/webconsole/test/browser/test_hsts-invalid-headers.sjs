/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  let issue;
  switch (request.queryString) {
    case "badSyntax":
      response.setHeader("Strict-Transport-Security", "\"");
      issue = "is not syntactically correct.";
      break;
    case "noMaxAge":
      response.setHeader("Strict-Transport-Security", "max-age444");
      issue = "does not include a max-age directive.";
      break;
    case "invalidIncludeSubDomains":
      response.setHeader("Strict-Transport-Security", "includeSubDomains=abc");
      issue = "includes an invalid includeSubDomains directive.";
      break;
    case "invalidMaxAge":
      response.setHeader("Strict-Transport-Security", "max-age=abc");
      issue = "includes an invalid max-age directive.";
      break;
    case "multipleIncludeSubDomains":
      response.setHeader("Strict-Transport-Security",
                         "includeSubDomains; includeSubDomains");
      issue = "includes multiple includeSubDomains directives.";
      break;
    case "multipleMaxAge":
      response.setHeader("Strict-Transport-Security",
                         "max-age=444; max-age=999");
      issue = "includes multiple max-age directives.";
      break;
  }

  response.write("This page is served with a STS header that " + issue);
}
