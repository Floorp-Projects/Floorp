<!-- -*- Mode: nXML; tab-width: 2; indent-tabs-mode: nil; -*- -->
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
   - The Original Code is Mozilla MathML Project.
   -
   - The Initial Developer of the Original Code is
   - Frederic Wang <fred.wang@free.fr>.
   - Portions created by the Initial Developer are Copyright (C) 2010
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
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  
  <xsl:strip-space elements="*"/>

  <xsl:template match="charlist">
    <root><xsl:apply-templates select="character"/></root>
  </xsl:template>

  <xsl:template match="character">
    <xsl:if test="operator-dictionary">
      <xsl:for-each select="operator-dictionary">
        <entry>

          <xsl:attribute name="unicode">
            <xsl:value-of select="../@id"/>
          </xsl:attribute> 

          <xsl:attribute name="form">
            <xsl:value-of select="@form"/>
          </xsl:attribute> 

          <!-- begin operator-dictionary -->
          <xsl:if test="@lspace">
            <xsl:attribute name="lspace">
              <xsl:value-of select="@lspace"/>
            </xsl:attribute> 
          </xsl:if>
          <xsl:if test="@rspace">
            <xsl:attribute name="rspace">
            <xsl:value-of select="@rspace"/>
            </xsl:attribute> 
          </xsl:if>
          <xsl:if test="@minsize">
            <xsl:attribute name="minsize">
              <xsl:value-of select="@minsize"/>
            </xsl:attribute> 
          </xsl:if>
          <xsl:if test="@*[.='true']">
            <xsl:attribute name="properties">
              <!-- largeop, movablelimits, stretchy, separator, accent, fence,
                   symmetric -->
              <xsl:for-each select="@*[.='true']">
                <xsl:value-of select="name()"/>
                <xsl:text> </xsl:text>
              </xsl:for-each>
            </xsl:attribute> 
          </xsl:if>
          <xsl:if test="@priority">
            <xsl:attribute name="priority">
              <xsl:value-of select="@priority"/>
            </xsl:attribute> 
          </xsl:if>
          <xsl:if test="@linebreakstyle">
            <xsl:attribute name="linebreakstyle">
              <xsl:value-of select="@linebreakstyle"/>
            </xsl:attribute> 
          </xsl:if>
          <!-- end operator-dictionary -->

          <xsl:attribute name="description">
            <xsl:value-of select="../description"/>
          </xsl:attribute> 

        </entry>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
