<?xml version="1.0"?>
<xsl:stylesheet
     xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!-- for read-ability use HTML output and enable indenting -->
<xsl:output method="html" indent="yes"/>

<!-- matches document node -->
<xsl:template match="/">
   <HTML>
      <HEAD>
         <TITLE>A simple sorting example</TITLE>
      </HEAD>
      <BODY>
         <P ALIGN="CENTER">
            <B>
               <FONT SIZE="+2">A simple sorting example</FONT>
               <HR SIZE="1"/>
            </B>
         </P>
         <!-- select and apply templates to remaining document -->
         <xsl:apply-templates/>
      </BODY>
   </HTML>
</xsl:template>

<!-- matches root element -->
<xsl:template match="/*">
   <TABLE BORDER="0">
   <TR><TD>Sorting in ascending order</TD></TR>
   <TR>
      <TD>
         <OL>
            <xsl:apply-templates>
               <!-- sort using the value of the selected node -->
               <xsl:sort select="."/>
            </xsl:apply-templates>
         </OL>
      </TD>
   </TR>
   <TR><TD>Sorting in descending order</TD></TR>
   <TR>
      <TD>
         <OL>
            <xsl:apply-templates>
               <!-- sort using the value of the selected node -->
               <xsl:sort select="." order="descending"/>
            </xsl:apply-templates>
         </OL>
      </TD>
   </TR>
   </TABLE>
</xsl:template>

<!-- matches only entry elements -->
<xsl:template match="entry">
    <LI><xsl:apply-templates/></LI>
</xsl:template>

</xsl:stylesheet>
