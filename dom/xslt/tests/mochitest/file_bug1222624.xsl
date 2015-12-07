<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:import href="file_bug1222624_sub.xsl"/>
  <xsl:template match="/root">
    <result>
      <xsl:call-template name="external"/>
      <xsl:value-of select="document('file_bug1222624_data1.xml')"/>
      <xsl:text>!</xsl:text>
      <xsl:value-of select="document(load)"/>
    </result>
  </xsl:template>
</xsl:stylesheet>
