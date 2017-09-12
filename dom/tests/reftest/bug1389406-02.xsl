<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
>

<xsl:template mode="title" match="*">
  <xsl:text> — TEST CASE</xsl:text>
  <xsl:apply-imports/>
</xsl:template>

<xsl:template match="/">
  <h3 xmlns="http://www.w3.org/1999/xhtml"><xsl:apply-templates select="title" mode="title"/></h3>
</xsl:template>

</xsl:stylesheet>
