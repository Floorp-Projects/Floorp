<?xml version="1.0"?>
<!--
/*
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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999, 2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    - original author.
 * Olivier Gerardin, ogerardin@vo.lu
 *    - added Number Function tests.
 */

/*
  This is a test stylesheet used for testing the TransforMiiX XSLT processor

 */
-->

<xsl:stylesheet
        xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
        result-ns="http://www.w3.org/1999/xhtml"
        default-space="strip"
        version="1.0">

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
    <TITLE>TransforMiiX Test Cases</TITLE>
  </HEAD>
  <BODY BGCOLOR="#FFFFFF" Text="#000000">
  <CENTER>
      <FONT COLOR="BLUE" FACE="Arial"><B>MITRE</B></FONT><BR/>
      <B>Transfor<FONT COLOR="blue">Mii</FONT>X Test Cases</B>
 </CENTER>
   <P ALIGN="CENTER">
       This document serves to test XPath and XSLT functions.
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
         String <B> local-name(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="local-name(names/abc:test-name)"/&gt;<BR />
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
            <xsl:value-of select="local-name(names/abc:test-name)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         String <B> local-name(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="local-name()"/&gt;<BR />
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
            <xsl:value-of select="local-name()"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         String <B> name(</B><I>node-set?</I><B>)</B>
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
         String<B> namespace-uri(</B><I>node-set?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="namespace-uri(names/abc:test-name)"/&gt;<BR />
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
            <xsl:value-of select="namespace-uri(names/abc:test-name)"/>
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
   <!-- ******************** -->
   <!-- * Number Functions * -->
   <!-- ******************** -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Number Functions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> number(</B><I>object?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="number()"/&gt;<BR />
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
            <xsl:value-of select="number()"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="number('654.97')"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">654.97</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="number('654.97')"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> round(</B><I>number</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="round(1.75)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">2</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="round(1.75)"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="round(1.25)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="round(1.25)"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="round(-0.5)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">0</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="round(-0.5)"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="round(0.5)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="round(0.5)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> floor(</B><I>number</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="floor(2.2)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">2</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="floor(2.2)"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="floor(-2.2)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">-3</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="floor(-2.2)"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>number</I><B> ceiling(</B><I>number</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="ceiling(2.2)"/&gt;<BR />
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
            <xsl:value-of select="ceiling(2.2)"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="ceiling(-2.2)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">-2</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="ceiling(-2.2)"/>
         </FONT>
      </TD>
   </TR>
   </TABLE>

   <!-- **************************** -->
   <!-- * XSLT Extension Functions * -->
   <!-- **************************** -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>XSLT Extension Functions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD BGColor="#EEEEEE"><B>Function:</B></TD>
      <TD BGColor="#EEEEEE">
         <I>string</I><B> generate-id(</B><I>NodeSet?</I><B>)</B>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="generate-id()"/&gt;<BR />
         <BR />
         <B>Note:</B>
         <UL>
            <FONT COLOR="red">
               This function will generate a unique id for the current node.
               I don't what this will be exactly, until run-time
            </FONT>
         </UL>
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">id{some-number}.0.1</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="generate-id()"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->

   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="generate-id(..)"/&gt;<BR />
         <B>Note:</B>
         <UL>
            <FONT COLOR="red">
               This function will generate a unique id for the parent of the current node.
               I don't what this will be exactly, until run-time
            </FONT>
         </UL>
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">id{some-number}</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="generate-id(..)"/>
         </FONT>
      </TD>
   </TR>

   </TABLE>

  </BODY>
</HTML>
</xsl:template>

</xsl:stylesheet>

