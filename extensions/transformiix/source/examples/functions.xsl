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
        result-ns="http://www.w3.org/TR/REC-html40"
        default-space="strip">

<!-- root rule -->
<xsl:template match="/">
   <xsl:apply-templates/>
</xsl:template>

<!-- supress non-selected nodes-->
<xsl:template match="*"/>

<!-- variable tests -->
<xsl:variable name="product-name">
   <B>Transfor<FONT COLOR="blue">Mii</FONT>X</B>
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
      This document serves to test XPath and XSL functions.
   </P>
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Boolean Functions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>boolean</I><B> boolean(</B><I>object</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="boolean(descendant::z)"/&gt;<BR />
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
         <FONT COLOR="blue">
            <xsl:value-of select="boolean(descendant::z)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="boolean(*)"/&gt;<BR />
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
         <FONT COLOR="blue">
            <xsl:value-of select="boolean(*)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>boolean</I><B> false()</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="false()"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">false</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="false()"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>boolean</I><B> true()</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="true()"/&gt;<BR />
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
         <FONT COLOR="blue">
            <xsl:value-of select="true()"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>boolean</I><B> not(</B><I>boolean</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="not(true())"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">false</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="not(true())"/>
         </FONT>
      </TD>
   </TR>
   </TABLE>
   <!-- ********************* -->
   <!-- * NodeSet Functions * -->
   <!-- ********************* -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>NodeSet Functions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> count(</B><I>node-set</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="count(*)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">4</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="count(*)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> position()</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="*[position()=3]"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">z</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="*[position()=3]"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> last()</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="*[last()-1]"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">z</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="*[last()-1]"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         String <B> local-part(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="local-part(names/abc:test-name)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">test-name</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="local-part(names/abc:test-name)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         String <B> local-part(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="local-part()"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">document</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="local-part()"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         String <B>name(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="name(names/abc:test-name)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">abc:test-name</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="name(names/abc:test-name)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         String <B>namespace(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="namespace(names/abc:test-name)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">abc</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="namespace(names/abc:test-name)"/>
         </FONT>
      </TD>
   </TR>

   </TABLE>
   <!-- ******************** -->
   <!-- * String Functions * -->
   <!-- ******************** -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>String Functions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> string(</B><I>object?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="string()"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">x y z</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="string()"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="string('xyz')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">xyz</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="string('xyz')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> concat(</B><I>string, string, string*</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="concat('abc', 'def')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">abcdef</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="concat('abc','def')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>boolean</I><B> contains(</B><I>string, string</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="contains('abcdef', 'efg')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">false</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="contains('abcdef', 'efg')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="contains('abcdef', 'bcd')"/&gt;<BR />
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
         <FONT COLOR="blue">
            <xsl:value-of select="contains('abcdef', 'bcd')"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>boolean</I><B> starts-with(</B><I>string, string</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="starts-with('abcdef', 'abc')"/&gt;<BR />
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
         <FONT COLOR="blue">
            <xsl:value-of select="starts-with('abcdef', 'abc')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="starts-with('abcdef', 'xyz')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">false</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="starts-with('abcdef', 'xyz')"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> string-length(</B><I>string?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="string-length(name())"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">8</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="string-length(name())"/>
         </FONT>
      </TD>
   </TR>
  <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="string-length('abcdef')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">6</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="string-length('abcdef')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test: substring() #1 -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> substring(</B><I>string, number, number?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring('12345', 1.5, 2.6)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">234</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring('12345', 1.5, 2.6)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test: substring() #2 -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring('12345', 0, 3)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">12</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring('12345', 0, 3)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test: substring() #3 -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring('12345', 0 div 0, 3)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue"></FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring('12345', 0 div 0, 3)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test: substring() #4 -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring('12345', 1, 0 div 0)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue"></FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring('12345', 1, 0 div 0)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test: substring() #5 -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring('12345', -42, 1 div 0)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">12345</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring('12345',-42, 1 div 0)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test: substring() #6 -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring('12345', -1 div 0, 1 div 0)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue"></FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring('12345', -1 div 0, 1 div 0)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> substring-after(</B><I>string, string</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring-after('1999/04/01', '/')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">04/01</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring-after('1999/04/01', '/')"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> substring-before(</B><I>string, string</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="substring-before('1999/04/01', '/')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1999</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="substring-before('1999/04/01', '/')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> translate(</B><I>string, string, string</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="translate('bar', 'abc', 'ABC')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">BAr</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="translate('bar', 'abc', 'ABC')"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="translate('---aaa---', 'abc-', 'ABC')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">AAA</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="translate('---aaa---', 'abc-', 'ABC')"/>
         </FONT>
      </TD>
   </TR>

   </TABLE>
  </BODY>
</HTML>
</xsl:template>

</xsl:stylesheet>

