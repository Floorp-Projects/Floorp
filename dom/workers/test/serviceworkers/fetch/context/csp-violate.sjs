function handleRequest(request, response)
{
  response.setHeader("Content-Security-Policy", "default-src 'none'; report-uri /tests/dom/workers/test/serviceworkers/fetch/context/csp-report.sjs", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write("<link rel=stylesheet href=style.css>");
}
