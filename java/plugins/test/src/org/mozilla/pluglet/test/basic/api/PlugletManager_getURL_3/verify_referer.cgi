#!/usr/local/bin/perl
$referer = $ENV{HTTP_REFERER};
print "Content-Type: text/html\n\n";
print "<html><head><script Language=\"JavaScript\">\n<!--\n";
print "function registerResults() \n{\n";
print "Packages.org.mozilla.pluglet.test.basic.api.PlugletManager_getURL_128.checkResult_referer(\"$referer\")\;\n";
print "}\n";
print "//-->\n";
print "</script></head><body onLoad=\"registerResults()\">\n";
print "This script make verification of HTTP_REFERER variable and tranfer it to pluglet\n";
print "</body></html>";
