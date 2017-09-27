<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.1"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
>

<xsl:import href="bug1389406-02.xsl"/>

<xsl:template mode="title" match="*">
  <xsl:value-of select="."/>
  <xsl:apply-imports />
</xsl:template>

<xsl:template match="text()|@*"></xsl:template>

</xsl:stylesheet>
