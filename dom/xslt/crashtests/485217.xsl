<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:key name="label" match="item2" use="undeclaredfunc()"/>

    <xsl:template match="root">
                    <xsl:for-each select="//item1">
                        <xsl:call-template name="item1" />
                    </xsl:for-each>
    </xsl:template>

    <xsl:template name="item1">
                <xsl:for-each select="key('label', 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA')">
                </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
