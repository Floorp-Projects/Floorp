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
   - The Original Code is xCal test tools - ecsJune.xcs.
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
<!-- ecsJune.xsl Version 0.8 -->
<!-- This stylesheet assumes you have xcs data for June 2002. It is easy to
   - modify it to generate other months. It would not be hard to get it to
   - take parameters. It is not the best way to display a calendar.
   - but it works. :-) -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:dt="http://xsltsl.org/date-time"
                version="1.0">

  <xsl:import href="date-time.xsl"/>
  <xsl:output method="html"/>

<xsl:template match="/">
<html>
<head>
<title>example Calendar</title>
<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1" />
<meta name="author" content="Gary Frederick" />
<meta name="description" content="example Calendar" />
<style type="text/css">
 div.c1 {text-align: center}
 td {height: 80px}
 span.calSum {font-family: monospace; font-size: 10pt}
 td.prevMonth {height: 80px}
</style>
</head>
<body>
<div class="c1">
<h2>June 2002</h2>

<table summary="example calendar"
cellpadding="2" cellspacing="2" border="1" width="100%">
<tbody>
<tr>
<th valign="top">Sun</th>
<th valign="top">Mon</th>
<th valign="top">Tue</th>
<th valign="top">Wed</th>
<th valign="top">Thu</th>
<th valign="top">Fri</th>
<th valign="top">Sat</th>
</tr>
<tr>
<td class="prevMonth" valign="top" width="14%">30<br/>
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020530')]" />
</span>
</td>
<td valign="top" width="14%">1<br/>
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020601')]" />
</span>
</td>
<td valign="top" width="14%">2<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020602')]" />
</span>
</td>
<td valign="top" width="14%">3<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020603')]" />
</span>
</td>
<td valign="top" width="14%">4<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020604')]" />
</span></td>
<td valign="top" width="14%">5<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020605')]" />
</span>
</td>
<td valign="top" width="14%">6<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020606')]" />
</span></td>
</tr>

<tr>
<td valign="top" width="14%">7<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020607')]" />
</span>
</td>
<td valign="top">8<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020608')]" />
</span>
</td>
<td valign="top">9<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020609')]" />
</span>
</td>
<td valign="top">10<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020610')]" />
</span>
</td>
<td valign="top">11<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020611')]" />
</span>
</td>
<td valign="top">12<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020612')]" />
</span></td>
<td valign="top">13<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020613')]" />
</span>
</td>
</tr>

<tr>
<td valign="top" width="14%">14<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020614')]" />
</span>
</td>
<td valign="top">15<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020615')]" />
</span>
</td>
<td valign="top">16<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020616')]" />
</span>
</td>
<td valign="top">17<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020617')]" />
</span>
</td>
<td valign="top">18<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020618')]" />
</span>
</td>
<td valign="top">19<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020619')]" />
</span>
</td>
<td valign="top">20<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020620')]" />
</span>
</td>
</tr>

<tr>
<td valign="top" width="14%">21<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020621')]" />
</span>
</td>
<td valign="top">22<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020622')]" />
</span>
</td>
<td valign="top">23<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020623')]" />
</span>
</td>
<td valign="top">24<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020624')]" />
</span>
</td>
<td valign="top">25<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020625')]" />
</span>
</td>
<td valign="top">26<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020626')]" />
</span>
</td>
<td valign="top">27<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020627')]" />
</span>
</td>
</tr>

<tr>
<td valign="top" width="14%">28<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020628')]" />
</span>
</td>
<td valign="top">29<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020629')]" />
</span>
</td>
<td valign="top">30<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020630')]" />
</span>
</td>
<td valign="top">31<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020631')]" />
</span>
</td>
<td class="prevMonth" valign="top">1<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020701')]" />
</span>
</td>
<td class="prevMonth" valign="top">2<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020702')]" />
</span>
</td>
<td class="prevMonth" valign="top">3<br />
<span class="calSum">
  <xsl:apply-templates select="//vevent[starts-with(dtstart, '20020703')]" />
</span>
</td>
</tr>
</tbody>
</table>

<br />
</div>

<br />
</body>
</html>
</xsl:template>


<xsl:template match="vevent">
<a href="http://www.jsoft.com/archive/taffie/events2002.html#{dtstamp}" title="{summary}"><xsl:value-of select="substring(summary, 1, 10)" /></a><br />
</xsl:template>


</xsl:stylesheet>
