<?xml version="1.0"?><!-- -*- Mode: xml; tab-width: 2; indent-tabs-mode: nil -*- -->
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
   - The Original Code is TransforMiiX XSLT processor code.
   -
   - The Initial Developer of the Original Code is
   - Keith Visco.
   - Portions created by the Initial Developer are Copyright (C) 2000
   - the Initial Developer. All Rights Reserved.
   -
   - Contributor(s):
   -   Keith Visco <kvisco@ziplink.net> (Original Author)
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

<!--
  This is a test stylesheet used for testing TransforMiiX
-->

<!DOCTYPE xsl:stylesheet [
  <!ENTITY cr "&#x0A;">
]>

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

