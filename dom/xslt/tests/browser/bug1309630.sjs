function handleRequest(request, response) {
  const XSLT = `<?xml version="1.0"?>
    <?xml-stylesheet type="text/xsl" href="#bug"?>
    <xsl:stylesheet id="bug" version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
      <xsl:template name="f" match="/">
        <xsl:param name="n" select="0" />
        <xsl:apply-templates select="document(concat('${request.path}?', $n))/xsl:stylesheet/xsl:template[@name='f']">
          <xsl:with-param name="n" select="$n + 1"/>
        </xsl:apply-templates>
        <xsl:call-template name="f">
          <xsl:with-param name="n" select="$n + 1"/>
        </xsl:call-template>
      </xsl:template>
    </xsl:stylesheet>`;

  // reset_counter sets the counter to -1.
  if (request.queryString === "reset_counter") {
    setState("base", "-1");
    response.write("");
    return;
  }

  // record_counter makes us store the current value of the counter.
  if (request.queryString === "record_counter") {
    setState("base", getState("counter"));
    response.write("");
    return;
  }

  // get_counter_change returns the difference between the current
  // value of the counter and the value it had when the script was
  // loaded with the record_counter query string.
  if (request.queryString === "get_counter_change") {
    response.write(
      String(Number(getState("counter")) - Number(getState("base")))
    );
    return;
  }

  // The XSLT code calls the document() function with a URL pointing to
  // this script, with the query string set to a counter starting from 0
  // and incremementing with every call of the document() function.
  // The first load will happen either from the xml-stylesheet PI, or
  // with fetch(), to parse a document to pass to
  // XSLTProcessor.importStylesheet. In that case the query string will
  // be empty, and we don't change the counter value, we only care about
  // the loads through the document() function.
  if (request.queryString) {
    setState("counter", request.queryString);
  }

  response.setHeader("Content-Type", "text/xml; charset=utf-8", false);
  response.write(XSLT);
}
