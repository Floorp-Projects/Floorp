<?xml version="1.0"?>
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!DOCTYPE overlay [
  <!ENTITY % prettyPrintDTD SYSTEM "chrome://global/locale/xml/prettyprint.dtd">
  %prettyPrintDTD;
  <!ENTITY % globalDTD SYSTEM "chrome://global/locale/global.dtd">
  %globalDTD;
]>

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml">

  <xsl:output method="xml"/>

  <xsl:template match="/">
    <div id="top">
      <link rel="stylesheet" href="chrome://global/content/xml/XMLPrettyPrint.css"/>
      <div id="header" dir="&locale.dir;">
        <p>
          &xml.nostylesheet;
        </p>
      </div>
      <main id="tree" class="highlight">
        <xsl:apply-templates/>
      </main>
    </div>
  </xsl:template>

  <xsl:template match="*">
    <div>
      <xsl:text>&lt;</xsl:text>
      <span class="start-tag"><xsl:value-of select="name(.)"/></span>
      <xsl:apply-templates select="@*"/>
      <xsl:text>/&gt;</xsl:text>
    </div>
  </xsl:template>

  <xsl:template match="*[node()]">
    <div>
      <xsl:text>&lt;</xsl:text>
      <span class="start-tag"><xsl:value-of select="name(.)"/></span>
      <xsl:apply-templates select="@*"/>
      <xsl:text>&gt;</xsl:text>

      <span class="text"><xsl:value-of select="."/></span>

      <xsl:text>&lt;/</xsl:text>
      <span class="end-tag"><xsl:value-of select="name(.)"/></span>
      <xsl:text>&gt;</xsl:text>
    </div>
  </xsl:template>

  <xsl:template match="*[* or processing-instruction() or comment() or string-length(.) &gt; 50]">
    <div>
      <details open="" class="expandable-body">
        <summary class="expandable-opening">
          <xsl:text>&lt;</xsl:text>
          <span class="start-tag"><xsl:value-of select="name(.)"/></span>
          <xsl:apply-templates select="@*"/>
          <xsl:text>&gt;</xsl:text>
        </summary>

        <div class="expandable-children"><xsl:apply-templates/></div>

      </details>
      <span class="expandable-closing">
        <xsl:text>&lt;/</xsl:text>
        <span class="end-tag"><xsl:value-of select="name(.)"/></span>
        <xsl:text>&gt;</xsl:text>
      </span>
    </div>
  </xsl:template>

  <xsl:template match="@*">
    <xsl:text> </xsl:text>
    <span class="attribute-name"><xsl:value-of select="name(.)"/></span>
    <xsl:text>=</xsl:text>
    <span class="attribute-value">"<xsl:value-of select="."/>"</span>
  </xsl:template>

  <xsl:template match="text()">
    <xsl:if test="normalize-space(.)">
      <xsl:value-of select="."/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="processing-instruction()">
    <div class="pi">
      <xsl:text>&lt;?</xsl:text>
      <xsl:value-of select="name(.)"/>
      <xsl:text> </xsl:text>
      <xsl:value-of select="."/>
      <xsl:text>?&gt;</xsl:text>
    </div>
  </xsl:template>

  <xsl:template match="processing-instruction()[string-length(.) &gt; 50]">
    <div class="pi">
      <details open="" class="expandable-body">
        <summary class="expandable-opening">
          <xsl:text>&lt;?</xsl:text>
          <xsl:value-of select="name(.)"/>
        </summary>
        <div class="expandable-children"><xsl:value-of select="."/></div>
      </details>
      <span class="expandable-closing">
        <xsl:text>?&gt;</xsl:text>
      </span>
    </div>
  </xsl:template>

  <xsl:template match="comment()">
    <div class="comment">
      <xsl:text>&lt;!--</xsl:text>
      <xsl:value-of select="."/>
      <xsl:text>--&gt;</xsl:text>
    </div>
  </xsl:template>

  <xsl:template match="comment()[string-length(.) &gt; 50]">
    <div class="comment">
      <details open="" class="expandable-body">
        <summary class="expandable-opening">
          <xsl:text>&lt;!--</xsl:text>
        </summary>
        <div class="expandable-children">
          <xsl:value-of select="."/>
        </div>
      </details>
      <span class="expandable-closing">
        <xsl:text>--&gt;</xsl:text>
      </span>
    </div>
  </xsl:template>

</xsl:stylesheet>
