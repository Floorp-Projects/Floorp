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
   - The Original Code is xCal test tools - xcs2ics.xcs.
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
<!-- xcs2ics.xsl Version 1.0 -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">
  <!-- <xsl:strip-space elements="*" /> -->
  <xsl:output method="text"/>

<xsl:template match="/">
<xsl:apply-templates select="iCalendar" />
</xsl:template>

<xsl:template match="iCalendar">
<xsl:apply-templates select="vcalendar" />

</xsl:template>

<xsl:template match="vcalendar">BEGIN:VCALENDAR
PRODID:<xsl:value-of select="prodid"/>
VERSION:<xsl:value-of select="version"/>
<xsl:apply-templates select="vevent" />
<xsl:apply-templates select="vjournal" />END:VCALENDAR
</xsl:template>


<xsl:template match="vevent">
BEGIN:VEVENT<xsl:apply-templates />
END:VEVENT
</xsl:template>

<xsl:template match="vjournal">
BEGIN:VJOURNAL
<xsl:apply-templates />
END:VJOURNAL
</xsl:template>

<xsl:template match="uid">
UID:<xsl:value-of select="."/>
</xsl:template>

<xsl:template match="summary">
SUMMARY:<xsl:value-of select="."/>
</xsl:template>

<xsl:template match="description">
DESCRIPTION:<xsl:value-of select="."/></xsl:template>

<xsl:template match="categories">
CATEGORIES:<xsl:for-each select="item"><xsl:value-of select="." /><xsl:if test="position() != last()">,</xsl:if></xsl:for-each></xsl:template>

<xsl:template match="priority">
PRIORITY:<xsl:value-of select="."/>
</xsl:template>
<xsl:template match="class">
CLASS:<xsl:value-of select="."/>
</xsl:template>
<xsl:template match="dtstamp">
DTSTAMP:<xsl:value-of select="."/>
</xsl:template>
<xsl:template match="sequence">
SEQUENCE:<xsl:value-of select="."/>
</xsl:template>

<xsl:template match="dtstart">
DTSTART<xsl:if test="@value">;VALUE=<xsl:value-of select="@value" />
      </xsl:if>:<xsl:value-of select="."/>
</xsl:template>
<xsl:template match="dtend">
DTEND:<xsl:value-of select="."/>
</xsl:template>

<xsl:template match="location">
LOCATION:<xsl:value-of select="."/></xsl:template>

<xsl:template match="*">
</xsl:template>
</xsl:stylesheet>
