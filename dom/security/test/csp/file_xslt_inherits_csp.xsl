<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.1">
    <xsl:output method="html"/>
    <xsl:variable name="title" select="/root/t"/>
    <xsl:template match="/">
    <html>
        <head>
            <title>
                <xsl:value-of select="$title"/>
            </title>
            <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
        </head>
        <body>
            <p>
                Below is some inline JavaScript generating some red text.
            </p>

            <p id="bug"/>
            <script>
               document.body.append("JS DID EXCECUTE");
            </script>

            <a onClick='document.body.append("JS DID EXCECUTE");' href="#">link with lineOnClick</a>
        </body>
    </html>
    </xsl:template>
</xsl:stylesheet>
