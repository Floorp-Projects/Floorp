<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:preserve-space elements="xalan sheet" />
<xsl:output method="xml" indent="yes" />
<xsl:template match="/xalan">
<xalantests>
  <xsl:for-each select="sheet">
  <xsl:message>Loading <xsl:value-of select='.' /></xsl:message>
  <xsl:variable name="stylesheet" select="document(concat(.,'.xsl'))" />
 <test>
    <xsl:attribute name="file"><xsl:value-of select="." /></xsl:attribute>
    <xsl:apply-templates select="$stylesheet//comment()" />
 </test>
  </xsl:for-each>
</xalantests>
</xsl:template>

<xsl:template match="comment()">
 <xsl:choose>
  <xsl:when test="starts-with(translate(.,'PUROSE','purose'),' purpose:')">
 <purpose>
    <xsl:value-of select="normalize-space(substring(.,11))" />
 </purpose>
    <xsl:variable name="cs"
                  select="normalize-space(following-sibling::comment())" />
    <xsl:if test="string-length($cs)
                  and not(starts-with($cs,'Author:'))
                  and not(starts-with($cs,'Creator:'))
                  and not(starts-with($cs,'ExpectedException:'))">
 <comment>
      <xsl:value-of 
          select="$cs" />
 </comment>
    </xsl:if>
  </xsl:when>
 </xsl:choose>
</xsl:template>
</xsl:stylesheet>
