var testType = 'synthetic';

var xslt = "<?xml version=\"1.0\"?> " +
           "<xsl:stylesheet version=\"1.0\"" +
           "   xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">" +
           "  <xsl:template match=\"node()|@*\">" +
           "    <xsl:copy>" +
           "      <xsl:apply-templates select=\"node()|@*\"/>" +
           "    </xsl:copy>" +
           "  </xsl:template>" +
           "  <xsl:template match=\"Error\"/>" +
           "</xsl:stylesheet>";

onfetch = function(event) {
  if (event.request.url.includes('test.xsl')) {
    if (testType == 'synthetic') {
      if (event.request.mode != 'cors') {
        event.respondWith(Response.error());
        return;
      }

      event.respondWith(Promise.resolve(
        new Response(xslt, { headers: {'Content-Type': 'application/xslt+xml'}})
      ));
    }
    else if (testType == 'cors') {
      if (event.request.mode != 'cors') {
        event.respondWith(Response.error());
        return;
      }

      var url = "http://example.com/tests/dom/workers/test/serviceworkers/xslt/xslt.sjs?" + escape(xslt);
      event.respondWith(fetch(url, { mode: 'cors' }));
    }
    else if (testType == 'opaque') {
      if (event.request.mode != 'cors') {
        event.respondWith(Response.error());
        return;
      }

      var url = "http://example.com/tests/dom/workers/test/serviceworkers/xslt/xslt.sjs?" + escape(xslt);
      event.respondWith(fetch(url, { mode: 'no-cors' }));
    }
    else {
      event.respondWith(Response.error());
    }
  }
};

onmessage = function(event) {
  testType = event.data;
};
