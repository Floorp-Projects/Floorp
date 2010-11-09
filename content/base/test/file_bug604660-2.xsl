<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">
  <xsl:template match="/">
    <html>
      <head>
        <title>XSLT script execution test</title>
      </head>
      <body>
        <script defer="" src="data:text/javascript,parent.scriptRan(5);"></script>
        <script>parent.scriptRan(1);</script>
        <script async="" src="data:text/javascript,parent.asyncRan();"></script>
        <script src="file_bug604660-3.js"></script>
        <script>parent.scriptRan(3);</script>
        <script src="file_bug604660-4.js"></script>        
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>

