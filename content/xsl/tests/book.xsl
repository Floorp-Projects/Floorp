<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template pattern="doc">
   <HTML>
   <HEAD>
     <TITLE>A Document</TITLE>
   </HEAD>
   <BODY>
      <xsl:process-children/>
   </BODY>
   </HTML>
</xsl:template>

<xsl:template pattern="title">
   <H1>
      <xsl:process-children/>
   </H1>
</xsl:template>
</xsl:stylesheet>