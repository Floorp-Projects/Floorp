<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
<html>
<head>
<script>
window.parent.frameScriptTag(document.readyState);

function attachCustomEventListener(element, eventName, command) {
    if (window.addEventListener &amp;&amp; !window.opera)
        element.addEventListener(eventName, command, true);
    else if (window.attachEvent)
        element.attachEvent("on" + eventName, command);
}

function load() {
  window.parent.frameLoad(document.readyState);
}

function readyStateChange() {
  window.parent.frameReadyStateChange(document.readyState);
}

function DOMContentLoaded() {
  window.parent.frameDOMContentLoaded(document.readyState);
}

window.onload=load;

attachCustomEventListener(document, "readystatechange", readyStateChange);
attachCustomEventListener(document, "DOMContentLoaded", DOMContentLoaded);

</script>
</head>
<body>
</body>
</html>
</xsl:template>

</xsl:stylesheet>