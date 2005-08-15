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
   - The Original Code is Feedview for Firefox.
   -
   - The Initial Developer of the Original Code is
   - Tom Germeau <tom.germeau@epigoon.com>.
   - Portions created by the Initial Developer are Copyright (C) 2004
   - the Initial Developer. All Rights Reserved.
   -
   - Contributor(s):
   -   Ben Goodger <ben@mozilla.org> 
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
                xmlns="http://www.w3.org/1999/xhtml"
                xmlns:date="http://www.jclark.com/xt/java/java.util.Date"
                xmlns:dc="http://purl.org/dc/elements/1.1/" version="1.0"
                xmlns:atom03="http://purl.org/atom/ns#"
                xmlns:atom10="http://www.w3.org/2005/Atom"
                xmlns:xul="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">

  <xsl:param name="url"/>
  <xsl:param name="articleCount"/>
  <xsl:param name="title"/>
  <xsl:param name="addLiveBookmarkLink"/>
  <xsl:param name="liveBookmarkInfoURL"/>
  <xsl:param name="liveBookmarkInfoText"/>
  <!-- these parameters are read from the Firefox preferences system and are stored
       as attributes on the <options> element in this page. -->
  <xsl:param name="showMenu"/>
  <xsl:param name="reloadInterval"/>
  
  <xsl:output method="html" media-type="text/html"/>
  <xsl:template match="/">
    <html>
      <head>
        <title>
          <xsl:value-of select="$title"/>
        </title>
        <link rel="stylesheet" href="chrome://browser/skin/feedview.css"/>
        <script type="application/x-javascript" src="chrome://browser/content/feedview.js"/>
      </head>
      <body>
        <!-- Menu box -->
        <div id="menubox">
          <span id="number">
            <xsl:value-of select="$articleCount"/>
          </span>
          <br/>
          <br/>
          <a href="javascript:void(0);" id="addLiveBookmarkLink">
            <xsl:attribute name="feed">
              <xsl:value-of select="$url"/>
            </xsl:attribute>
            <xsl:value-of select="$addLiveBookmarkLink"/>
          </a>
          <br/>
          <br/>
          <a id="liveBookmarkInfoLink">
            <xsl:attribute name="href">
              <xsl:value-of select="$liveBookmarkInfoURL"/>
            </xsl:attribute>
            <xsl:value-of select="$liveBookmarkInfoText"/>
          </a>
        </div>
        
        <!-- The options place holder -->
        <div id="data" style="display: none;">
          <xsl:attribute name="showmenu">
            <xsl:value-of select="$showMenu"/>
          </xsl:attribute>
          <xsl:attribute name="reloadinterval">
            <xsl:value-of select="$reloadInterval"/>
          </xsl:attribute>
          <xsl:attribute name="url">
            <xsl:value-of select="$url"/>
          </xsl:attribute>
        </div>

        <xsl:apply-templates/>
        
        <script type="application/x-javascript">
          // Initialize the view
          FeedView.init();
        </script>
      </body>
    </html>
  </xsl:template>
  <!-- RSS RDF Support-->
  <xsl:template name="a-element">
    <xsl:element name="a">
      <xsl:attribute name="href">
        <xsl:apply-templates select="*[local-name()='link']"/>
      </xsl:attribute>
      <xsl:value-of select="*[local-name()='title']"/>
    </xsl:element>
  </xsl:template>
  <xsl:template match="*[local-name()='channel']">
    <h1>
      <xsl:call-template name="a-element"/>
    </h1>
    <!-- Following line for RSS .091 -->
    <div id="articles">
      <xsl:apply-templates select="*[local-name()='item']"/>
    </div>
  </xsl:template>
  <xsl:template match="*[local-name()='image']"></xsl:template>
  <xsl:template match="*[local-name()='textinput']"></xsl:template>
  <xsl:template match="*[local-name()='item']">
    <div class="article">
      <xsl:if test="dc:date">
        <xsl:attribute name="date">
          <xsl:value-of select="dc:date" />
        </xsl:attribute>
      </xsl:if>
      <xsl:if test="*[local-name()='pubDate']">
        <xsl:attribute name="date">
          <xsl:value-of select="*[local-name()='pubDate']" />
        </xsl:attribute>
      </xsl:if>
      <h2><xsl:call-template name="a-element"/></h2>
      <span class="date"></span>
      <xsl:value-of select="*[local-name()='description']" />
      <p></p>
    </div>
  </xsl:template>

  <!-- ATOM 0.3 SUPPORT-->
  <xsl:template match="atom03:feed">
    <h1>
      <a href="{atom03:link[substring(@rel, 1, 8)!='service.']/@href}">
        <xsl:value-of select="atom03:title"/>
      </a>
    </h1>
    <div id="articles">
      <xsl:apply-templates select="atom03:entry"/>
    </div>
  </xsl:template>
  <xsl:template match="atom03:entry">
    <div class="article">
      <xsl:if test="atom03:issued">
        <xsl:attribute name="date">
          <xsl:value-of select="atom03:issued"/>
        </xsl:attribute>
      </xsl:if>
      <a href="{atom03:link[substring(@rel, 1, 8)!='service.']/@href}">
        <h2><xsl:value-of select="atom03:title"/></h2>
      </a>
      <span class="date"/>
      <xsl:choose>
        <xsl:when test="atom03:content">
          <xsl:value-of select="atom03:content"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="atom03:summary"/>
        </xsl:otherwise>
      </xsl:choose>
      <p/>
    </div>
  </xsl:template>
  
  <!-- ATOM 1.0 SUPPORT-->
  <xsl:template match="atom10:feed">
    <h1>
      <a href="{atom10:link[substring(@rel, 1, 8)!='service.']/@href}">
        <xsl:value-of select="atom10:title"/>
      </a>
    </h1>
    <div id="articles">
      <xsl:apply-templates select="atom10:entry"/>
    </div>
  </xsl:template>
  <xsl:template match="atom10:entry">
    <div class="article">
      <xsl:if test="atom10:updated">
        <xsl:attribute name="date">
          <xsl:value-of select="atom10:updated"/>
        </xsl:attribute>
      </xsl:if>
      <a href="{atom10:link[substring(@rel, 1, 8)!='service.']/@href}">
        <h2><xsl:value-of select="atom10:title"/></h2>
      </a>
      <span class="date"/>
      <xsl:choose>
        <xsl:when test="atom10:content">
          <xsl:value-of select="atom10:content"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="atom10:summary"/>
        </xsl:otherwise>
      </xsl:choose>
      <p/>
    </div>
  </xsl:template>
    
</xsl:stylesheet>
