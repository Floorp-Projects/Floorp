<?xml version="1.0"?>
<!--
/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * This is a test stylesheet used for testing MITRE's XSL processor
**/
-->
<xsl:stylesheet
        xmlns:xsl="http://www.w3.org/XSL/Transform/1.0"
        result-ns="http://www.w3.org/TR/REC-html40">

<!-- AttributeSet -->
<xsl:attribute-set name="style1">
   <xsl:attribute name="COLOR">blue</xsl:attribute>
   <xsl:attribute name="SIZE">+0</xsl:attribute>
</xsl:attribute-set>

<xsl:attribute-set name="style2">
   <xsl:attribute name="COLOR">red</xsl:attribute>
   <xsl:attribute name="SIZE">+0</xsl:attribute>
</xsl:attribute-set>

<!-- root rule -->
<xsl:template match="/">
   <xsl:processing-instruction name="foo">
      this is a test processing ?> instruction
   </xsl:processing-instruction>
   <xsl:comment>MITRE TransforMiiX Test cases, written by Keith Visco.</xsl:comment>
   <xsl:apply-templates/>
</xsl:template>

<!-- named template -->
<xsl:template name="named-template-test">
  named template processed!
</xsl:template>

<!-- supress non-selected nodes-->
<xsl:template match="*"/>

<!-- variable tests -->
<xsl:variable name="product-name">
  Transfor<FONT COLOR="blue">Mii</FONT>X
</xsl:variable>
<!-- main rule for document element -->
<xsl:template match="document">
<HTML>
  <HEAD>
    <TITLE>MII TransforMiiX Test Cases</TITLE>
  </HEAD>
  <BODY>
  <CENTER>
      <FONT COLOR="BLUE" FACE="Arial"><B>MITRE</B></FONT><BR/>
      <B>MII Transfor<FONT COLOR="blue">Mii</FONT>X Test Cases</B>
 </CENTER>
 <P>
  This document serves to test basic XSL expressions.
 </P>
 <!-- new test -->
 <P>
    <B>Testing xsl:variable</B><BR/>
    <B>Test:</B> &lt;xsl:value-of select="$product-name"/&gt;<BR/>
    <B>Desired Result:</B>TransforMiiX<BR/>
    <B>Result:</B><xsl:value-of select="$product-name"/>
 </P>
 <!-- new test -->
 <P>
    <B>Testing xsl:if</B><BR/>
    <B>Test:</B> &lt;xsl:if test="x | y | z"&gt;true&lt;/xsl:if&gt;<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B> <xsl:if test="x | y | z">true</xsl:if>
 </P>

 <!-- new test -->
 <P>
    <B>Testing xsl:choose</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B>
    <xsl:choose>
        <xsl:when test="a">error - a</xsl:when>
        <xsl:when test="abc/def">true</xsl:when>
        <xsl:when test="b">error - b</xsl:when>
        <xsl:otherwise>false</xsl:otherwise>
    </xsl:choose>
 </P>
 <!-- new test -->
 <P>
    <B>Testing parent and ancestor ops</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B><xsl:if test="//def">true</xsl:if><BR/>

 </P>
 <!-- new test -->

  <P>
    <B>Testing basic xsl:apply-templates</B><BR/>
    <B>Test:</B>&lt;xsl:apply-templates/&gt;<BR/>
    <B>Desired Result:</B>element x, element y, element z<BR/>
    <B>Result:</B><xsl:apply-templates/>
  </P>
 <!-- new test -->

  <P>
    <B>Testing basic xsl:apply-templates with mode</B><BR/>
    <B>Test:</B>&lt;xsl:apply-templates mode="mode-test"/&gt;<BR/>
    <B>Desired Result:</B>x, y, z<BR/>
    <B>Result:</B><xsl:apply-templates mode="mode-test"/>
  </P>
 <!-- new test -->
  <P>
    <B>Testing predicates</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B> <B>z</B><BR/>
    <B>Result:</B>
    <xsl:for-each select="*[position()=3]">
      <B><xsl:value-of select="."/></B>
    </xsl:for-each>
  </P>
 <!-- new test -->
  <P>
    <B>Testing predicates</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B><BR/>
    <B>Result:</B>
    <xsl:for-each select="*[false()]">
      <B><xsl:value-of select="."/></B>
    </xsl:for-each>
  </P>
 <!-- new test -->
  <P>
    <B>Named Template/Call Template</B><BR/>
    <B>Test:</B>&lt;xsl:call-template name="named-template-test"/&gt;<BR/>
    <B>Desired Result:</B>named template processed!<BR/>
    <B>Result:</B><xsl:call-template name="named-template-test"/>
  </P>
 <!-- new test -->
  <P>
    <B>Attribute Value Templates and variables</B><BR/>
    <B>Test:</B>
    <UL>
       &lt;xsl:variable name="color"&gt;red&lt;/xsl:variable&gt;<BR/>
       &lt;FONT COLOR="{$color}"&gt;Red Text&lt;/FONT&gt;
    </UL>
    <B>Desired Result:</B>
        <FONT COLOR="red">Red Text</FONT><BR/>
    <B>Result:</B>
       <xsl:variable name="color">red</xsl:variable>
       <FONT COLOR="{$color}">Red Text</FONT>
  </P>
  <HR/>
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Axis Identifiers (these should work, I need more test cases though)</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:if test="descendant::z"&gt;true&lt;/xsl:if&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">true</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:if test="descendant::z">
           <FONT COLOR="blue">true</FONT>
         </xsl:if>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:if test="not(descendant-or-self::no-element)"&gt;true&lt;/xsl:if&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">true</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:if test="not(descendant-or-self::no-element)">
           <FONT COLOR="blue">true</FONT>
         </xsl:if>
      </TD>
   </TR>
   </TABLE>

  <HR/>
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Creating Elements with xsl:element and xsl:attribute</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:element name="FONT"&gt;<BR />
         &lt;xsl:attribute name="COLOR"&gt;blue&lt;/xsl:attribute&gt; <BR/>
         Passed <BR/>
         &lt;/xsl:element&gt;
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">Passed</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:element name="FONT">
           <xsl:attribute name="COLOR">blue</xsl:attribute>
           Passed
         </xsl:element>
      </TD>
   </TR>
   <!-- new test -->
   <TR BGCOLOR="#E0E0FF" ALIGN="CENTER">
      <TD COLSPAN="2"><B>Using Attribute Sets</B></TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;FONT xsl:use-attribute-sets="style1"&gt;<BR />
         Passed <BR/>
         &lt;/FONT&gt;
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">Passed</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT xsl:use-attribute-sets="style1">
            Passed
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:element name="FONT" use-attribute-sets="style1 style2"&gt;<BR />
         Passed <BR/>
         &lt;/xsl:element&gt;
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="red">Passed</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:element name="FONT" use-attribute-sets="style1 style2">
            Passed
         </xsl:element>
      </TD>
   </TR>
   </TABLE>

  <HR/>
  <!-- ADDITIVE EXPRESSION TESTS -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Additive Expressions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="70+4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">74</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="70+4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="-70+4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">-66</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="-70+4"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="1900+70+8-4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1974</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="1900+70+8-4"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="(4+5)-(9+9)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">-9</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="(4+5)-(9+9)"/>
         </FONT>
      </TD>
   </TR>

  </TABLE>
  <HR/>
  <!-- MULTIPLICATIVE EXPRESSION TESTS -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Multiplicative Expressions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7*4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">28</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7*4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7mod4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">3</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7mod4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7div4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1.75</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7div4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7div0"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">Infinity</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7div0"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="0 div 0"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">NaN</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="0 div 0"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:variable name="x" expr="7*3"/&gt;<BR />
         &lt;xsl:variable name="y" expr="3"/&gt;<BR />
         &lt;xsl:value-of select="$x div $y"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">7</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:variable name="x" expr="7*3"/>
            <xsl:variable name="y" expr="3"/>
            <xsl:value-of select="$x div $y"/>
         </FONT>
      </TD>
   </TR>
   </TABLE>
  </BODY>
</HTML>
</xsl:template>

<!-- simple union expressions -->
<xsl:template match="x | y | z" priority="1.0">
   <xsl:if test="not(position()=1)">,</xsl:if>
   element<B><xsl:text> </xsl:text><xsl:value-of select="@*"/></B>
</xsl:template>

<xsl:template match="x | y | z" mode="mode-test">
   <xsl:if test="not(position()=1)"><xsl:text>, </xsl:text></xsl:if>
   <xsl:value-of select="@*"/>
</xsl:template>

<xsl:template match="z">
   element (z): <B><xsl:value-of select="."/></B>
</xsl:template>

</xsl:stylesheet>

