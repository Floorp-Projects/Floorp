<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:template match="root">
    <html><body><xsl:apply-templates select="item" /></body></html>
  </xsl:template>
  <xsl:template match="item">
    <img>
      <xsl:attribute name="src">
        <xsl:value-of select="@name" />
      </xsl:attribute>
    </img>
  </xsl:template>
</xsl:stylesheet>
