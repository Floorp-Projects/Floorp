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
   - The Original Code is xCal test tools - xcs2rdf.xsl.
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
<!-- Version 0.2 -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:output method="xml"/>


<xsl:template match="/">
<rdf:RDF
  xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'
  xmlns='http://www.w3.org/2000/10/swap/pim/ical#'
  xmlns:i='http://www.w3.org/2000/10/swap/pim/ical#'>
  <Vcalendar rdf:about=''>
    <version><xsl:value-of select="iCalendar/vcalendar/version" /></version>
    <prodid><xsl:value-of select="iCalendar/vcalendar/prodid" /></prodid>
  </Vcalendar>
  <xsl:apply-templates select="iCalendar/vcalendar/vtodo | iCalendar/vcalendar/vevent" /></rdf:RDF>
</xsl:template>

<xsl:template match="vtodo">
<Vtodo>
  <uid><xsl:value-of select="uid" /></uid>
  <summary><xsl:value-of select="summary" /></summary>
  <description><xsl:value-of select="description" /></description>
  <categories><xsl:value-of select="categories" /></categories>
  <status><xsl:value-of select="status" /></status>
  <class rdf:resource='http://www.w3.org/2000/10/swap/pim/ical#private'/>
<!-- ??? -->
  <dtstart rdf:parseType='Resource'>
    <value><xsl:value-of select="dtstart" /></value>
  </dtstart>
  <dtstamp><xsl:value-of select="dtstamp" /></dtstamp>
  <due><xsl:value-of select="due" /></due>
</Vtodo>
</xsl:template>

<xsl:template match="vevent">
<Vevent>
  <uid><xsl:value-of select="uid" /></uid>
  <summary><xsl:value-of select="summary" /></summary>
  <description><xsl:value-of select="description" /></description>
  <categories><xsl:value-of select="categories" /></categories>
  <status><xsl:value-of select="status" /></status>
  <class rdf:resource='http://www.w3.org/2000/10/swap/pim/ical#private'/>
<!-- ??? -->
  <dtstart rdf:parseType='Resource'>
    <value><xsl:value-of select="dtstart" /></value>
  </dtstart>
  <dtstamp><xsl:value-of select="dtstamp" /></dtstamp>
  <due><xsl:value-of select="due" /></due>
</Vevent>
</xsl:template>

</xsl:stylesheet>
