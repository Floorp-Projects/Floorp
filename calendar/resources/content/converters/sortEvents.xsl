<?xml version="1.0"?> 
<!-- ***** BEGIN LICENSE BLOCK *****
   - Version: MPL 1.1/GPL 2.0/LGPL 2.1
   -
   - The contents of this file are subject to the Mozilla Public License Version
   - 1.1 (the "License"); you may not use this file except in compliance with
   - the License. You may obtain a copy of the License at
   - http://www.mozilla.org/MPL/
   -
   - Software distributed under the License is distributed on an "AS IS" basis,
   - WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
   - for the specific language governing rights and limitations under the
   - License.
   -
   - The Original Code is sortEvents.xsl.
   -
   - The Initial Developer of the Original Code is
   - Jefferson Software Inc.
   - Portions created by the Initial Developer are Copyright (C) 2002
   - the Initial Developer. All Rights Reserved.
   -
   - Contributor(s): Gary Frederick <gary.frederick@jsoft.com>
   -
   - Alternatively, the contents of this file may be used under the terms of
   - either the GNU General Public License Version 2 or later (the "GPL"), or
   - the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
   - in which case the provisions of the GPL or the LGPL are applicable instead
   - of those above. If you wish to allow use of your version of this file only
   - under the terms of either the GPL or the LGPL, and not to allow others to
   - use your version of this file under the terms of the MPL, indicate your
   - decision by deleting the provisions above and replace them with the notice
   - and other provisions required by the LGPL or the GPL. If you do not delete
   - the provisions above, a recipient may use your version of this file under
   - the terms of any one of the MPL, the GPL or the LGPL.
   -
   - ***** END LICENSE BLOCK ***** --> 

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:dt="http://xsltsl.org/date-time"
                version="1.0">

  <xsl:import href="date-time.xsl"/>
  <xsl:output method="html"/>


<xsl:template match="/">
<html>
<body>
  <xsl:for-each select="//vevent">
    <xsl:sort select="substring(dtstart, 1, 8)" />
    <p><xsl:attribute name="id"><xsl:value-of select="uid" /></xsl:attribute>
    Event: <xsl:apply-templates select="summary"/>
    <br />
    Date: <xsl:apply-templates select="dtstart"/><br />Location: <xsl:value-of select="location" />
    <br />
    <xsl:apply-templates select="description" />
    </p>

  </xsl:for-each>
</body>
</html>
</xsl:template>

<xsl:template match="dtstart">
  <xsl:call-template name="DATE">
    <xsl:with-param name="this-date" select="."/>
  </xsl:call-template>
</xsl:template>


<xsl:template name="DATE">
  <xsl:param name="this-date"/>

  <xsl:call-template name="dt:format-date-time">
    <xsl:with-param name="year" select='substring($this-date, 1, 4)' />
    <xsl:with-param name="month" select='substring($this-date, 5, 2)'/>
    <xsl:with-param name="day" select='substring($this-date, 7, 2)'/>
    <xsl:with-param name="hour">10</xsl:with-param>
    <xsl:with-param name="minute">30</xsl:with-param>
    <xsl:with-param name="second">0</xsl:with-param>
    <xsl:with-param name="time-zone"></xsl:with-param>
    <xsl:with-param name="format" select="'%B %d, %Y'" />
  </xsl:call-template>
</xsl:template>


<xsl:template match="description">
  <xsl:apply-templates />
</xsl:template>

<xsl:template match="br">
  <br/>
</xsl:template>

<xsl:template match="a">
  <a>
    <xsl:attribute name="href"><xsl:value-of select="." /></xsl:attribute>
    <xsl:value-of select="." />
  </a>
</xsl:template>

<xsl:template match="address">
  <a>
      <xsl:if test="@kind='mailto'">
        <xsl:attribute name="href">mailto:<xsl:value-of select="." /></xsl:attribute>
      </xsl:if>
      <xsl:if test="@kind='http'">
        <xsl:attribute name="href">http://<xsl:value-of select="." /></xsl:attribute>
      </xsl:if>
    <xsl:value-of select="." />
  </a><br/>
</xsl:template>

</xsl:stylesheet>
