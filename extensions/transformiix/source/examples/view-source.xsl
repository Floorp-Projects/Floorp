<?xml version="1.0"?>
<!DOCTYPE xsl:stylesheet [
  <!ENTITY cr "&#x0A;">
]>

<!--
/**
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * Portions created by Keith Visco (C) 2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    - original author.
 *
 *
 *
 * This is a test stylesheet used for testing TransforMiiX
**/
-->
<xsl:stylesheet
        xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
        version="1.0">

<!-- root template -->
<xsl:template match="/">
   <HTML>
   <HEAD>
      <TITLE>View Source:</TITLE>
   </HEAD>
   <BODY>
      <!-- Add XML Declaration (as long as we have child nodes!) -->
      <xsl:if test="node()">
         <B>&lt;?xml version=&quot;1.0&quot;?&gt;</B><BR/>
      </xsl:if>
      <xsl:apply-templates select="node()"/>
   </BODY>
   </HTML>
</xsl:template>

<!-- comment template -->
<xsl:template match="comment()">
   <I>&lt;!--<xsl:value-of select="."/>--&gt;</I>
</xsl:template>

<!-- generic element template -->
<xsl:template match="*">

   <!-- open element tag -->
   <FONT COLOR="blue">&lt;</FONT><FONT COLOR="red"><xsl:value-of select="name(.)"/></FONT>
   <xsl:apply-templates select="@*"/><FONT COLOR="blue">&gt;</FONT>

   <!-- add break if we have element children -->
   <xsl:if test="*"><BR/></xsl:if>

   <!-- recursively apply templates to children of this element -->
   <xsl:apply-templates/>

   <!-- close element tag -->
   <FONT COLOR="blue">&lt;/</FONT>
   <FONT COLOR="red"><xsl:value-of select="name(.)"/></FONT>
   <FONT COLOR="blue">&gt;</FONT>
   <BR/>
</xsl:template>

<!-- generic attribute template -->
<xsl:template match="@*">
   &#160;<B><xsl:value-of select="name(.)"/></B>
   <FONT COLOR="green">=&quot;<xsl:value-of select="."/>&quot;</FONT>
</xsl:template>

<!-- processing instruction template -->
<xsl:template match="processing-instruction()">
  <I>&lt;?<xsl:value-of select="name(.)"/>&#160;<xsl:value-of select="."/>?&gt;</I><BR/>
</xsl:template>


</xsl:stylesheet>

