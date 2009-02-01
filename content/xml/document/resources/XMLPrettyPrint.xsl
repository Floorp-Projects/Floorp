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
   - The Original Code is mozilla.org code.
   -
   - The Initial Developer of the Original Code is
   - Netscape Communications Corporation.
   - Portions created by the Initial Developer are Copyright (C) 2002
   - the Initial Developer. All Rights Reserved.
   -
   - Contributor(s):
   -   Jonas Sicking <sicking@bigfoot.com> (Original author)
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
    <link href="chrome://global/content/xml/XMLPrettyPrint.css" type="text/css" rel="stylesheet"/>
    <link title="Monospace" href="chrome://global/content/xml/XMLMonoPrint.css" type="text/css" rel="alternate stylesheet"/>
    <div id="header" dir="&locale.dir;">
      <p>
        &xml.nostylesheet;
      </p>
    </div>
    <xsl:apply-templates/>
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
    <div class="expander-open">
      <xsl:call-template name="expander"/>

      <xsl:text>&lt;</xsl:text>
      <span class="start-tag"><xsl:value-of select="name(.)"/></span>
      <xsl:apply-templates select="@*"/>
      <xsl:text>&gt;</xsl:text>

      <div class="expander-content"><xsl:apply-templates/></div>

      <xsl:text>&lt;/</xsl:text>
      <span class="end-tag"><xsl:value-of select="name(.)"/></span>
      <xsl:text>&gt;</xsl:text>
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
    <div class="expander-open">
      <xsl:call-template name="expander"/>

      <span class="pi">
        <xsl:text> &lt;?</xsl:text>
        <xsl:value-of select="name(.)"/>
      </span>
      <div class="expander-content pi"><xsl:value-of select="."/></div>
      <span class="pi">
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
    <div class="expander-open">
      <xsl:call-template name="expander"/>

      <span class="comment">
        <xsl:text>&lt;!--</xsl:text>
      </span>
      <div class="expander-content comment">
        <xsl:value-of select="."/>
      </div>
      <span class="comment">
        <xsl:text>--&gt;</xsl:text>
      </span> 
    </div>
  </xsl:template>
  
  <xsl:template name="expander">
    <div class="expander">&#x2212;</div>
  </xsl:template>

</xsl:stylesheet>
