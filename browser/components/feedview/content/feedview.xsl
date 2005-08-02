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

  <!-- 
    These parameters are filled in by the overlay.js with processor.setParameter
    The parameters are used in '<options id="options" ' so they can be accessed in the DOM from JS
    - there should be an easier way getting the settings
  -->
  <xsl:param name="feedUrl"/>
  <xsl:param name="feedDescription"/>
  <xsl:param name="feedTitle"/>
  <xsl:param name="lengthSliderLabel"/>
  <xsl:param name="articleLength"/>
  <xsl:param name="showBar"/>
  <xsl:param name="showImage"/>
  <xsl:param name="timerInterval"/>
  <xsl:param name="externalCSS"/>
  
  <xsl:output method="html" media-type="text/html"/>
  <xsl:template match="/">
    <html>
      <head>
        <link rel="alternate" type="application/rss+xml" title="Current Feed" href=""/>
        <title>
          <xsl:value-of select="$feedTitle"/>
        </title>
        <link rel="stylesheet" href="chrome://browser/skin/feedview/feedview.css"/>

        <!-- The custom stylesheet from the settings-->
        <link rel="stylesheet">
          <xsl:attribute name="href">
            <xsl:value-of select="$externalCSS"/>
          </xsl:attribute>
        </link>

        <script type="application/x-javascript" src="chrome://browser/content/feedview/feedview.js"/>
      </head>
      <body>
        <!-- Menu box -->
        <div id="menubox">
          <xsl:value-of select="$lengthSliderLabel"/>
          <br/>
          <table>
            <tr id="lengthSwitcher">
              <td>
                <a onclick="javascript:updateArticleLength(100);">
                  <div style="height: 10px;"/>
                </a>
              </td>
              <td>
                <a onclick="javascript:updateArticleLength(300);">
                  <div style="margin-left: 8px; margin-right: 8px; height: 15px; "/>
                </a>
              </td>
              <td>
                <a onclick="javascript:updateArticleLength(900);">
                  <div style="height: 20px; "/>
                </a>
              </td>
            </tr>
          </table>
          
          
          <xul:slider id="lengthSlider"  style="width: 110px; " flex="1"
                      pageincrement="10" maxpos="100" 
                      onmousemove="updateArticleLength(this.getAttribute('curpos'))" 
                      onclick="updateArticleLength(this.getAttribute('curpos'))" 
                      onmouseup="updateArticleLength(this.getAttribute('curpos'))">
            <xsl:attribute name="curpos">
              <xsl:value-of select="$articleLength"/>
            </xsl:attribute>
            <xul:thumb/>
          </xul:slider>
          <br/>
          <br/>
          <span id="number">
            <xsl:value-of select="$feedDescription"/>
          </span>
          <br/>
          <br/>
          <span id="timerspan">Refresh in <span id="timevalue">
            <xsl:value-of select="$timerInterval"/>
          </span> seconds</span>
        </div>
        
        <!-- The options place holder -->
        <options id="options" style="display:none">
          <xsl:attribute name="showbar">
            <xsl:value-of select="$showBar"/>
          </xsl:attribute>
          <xsl:attribute name="showimage">
            <xsl:value-of select="$showImage"/>
          </xsl:attribute>
          <xsl:attribute name="timerinterval">
            <xsl:value-of select="$timerInterval"/>
          </xsl:attribute>
        </options>
        <xsl:apply-templates/>
        
        <!-- Dynamic stuff -->
        <script><![CDATA[
          if (document.getElementById("options").getAttribute("showbar") != "true")
            document.getElementById("menubox").style.display = "none";

          var showImage = true;
          if (document.getElementById("options").getAttribute("showimage") != "true")
            showImage = false; 

          setDate(); 
          updateArticleLength(document.getElementById("lengthSlider").getAttribute("curpos"), showImage); 
          setFeed(); 
          
          var interval = document.getElementById("options").getAttribute("timerinterval");
            
          // only needed for automatic refresh
          function refresh() {
            interval -= 1; 
            if (interval <= 0) 
              window.location.href = window.location.href;
            else
              document.getElementById("timevalue").textContent = interval;
            setTimeout("refresh()", 1000);
          }

          // if there is no interval hide the ticker
          if (interval > 0)
            setTimeout("refresh()", 1000);
          else
            document.getElementById("timerspan").style.display = "none";
        ]]></script>
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
      <xsl:attribute name="description">
        <xsl:value-of select="*[local-name()='description']" />
      </xsl:attribute>
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
      <xsl:call-template name="a-element"/>
      <span class="date"></span>

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
      <xsl:attribute name="description">
        <xsl:choose>
          <xsl:when test="atom03:content">
            <xsl:value-of select="atom03:content"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="atom03:summary"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:attribute>
      <a href="{atom03:link[substring(@rel, 1, 8)!='service.']/@href}">
        <xsl:value-of select="atom03:title"/>
      </a>
      <span class="date"/>
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
      <xsl:attribute name="description">
        <xsl:choose>
          <xsl:when test="atom10:content">
            <xsl:value-of select="atom10:content"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="atom10:summary"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:attribute>
      <a href="{atom10:link[substring(@rel, 1, 8)!='service.']/@href}">
        <xsl:value-of select="atom10:title"/>
      </a>
      <span class="date"/>
      <p/>
    </div>
  </xsl:template>
    
</xsl:stylesheet>
