<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:key name="polyList" match="quad" use="@id" />

  <xsl:template match="root">
    <html><body><xsl:apply-templates select="category" /></body></html>
  </xsl:template>

  <xsl:template match="category">
    <table><xsl:apply-templates select="list" /></table>
  </xsl:template>

  <xsl:template match="list">
    <tr><td><xsl:apply-templates select="key('polyList',@item)" /></td></tr>
  </xsl:template>

  <xsl:template match="quad">
    <b>Please output something!</b>
  </xsl:template>
</xsl:stylesheet>
