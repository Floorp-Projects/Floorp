/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS  IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. 
* All Rights Reserved.
*
* Contributor(s): bedney@technicalpursuit.com, pschwartau@netscape.com
* Date: 04 October 2001
*
* SUMMARY:  Arose from Bugzilla bug 103087:
* "The RegExp MarkupSPE in demo crashes Mozilla"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=103087
* SpiderMonkey crashed while executing this RegExp -
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 103087;
var summary = "Testing that we don't crash on this regexp -";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// here's a string to test the regexp on -
var str = ''; 
str += '<html xmlns="http://www.w3.org/1999/xhtml"' + '\n';
str += '      xmlns:xlink="http://www.w3.org/XML/XLink/0.9">' + '\n';
str += '  <head><title>Three Namespaces</title></head>' + '\n';
str += '  <body>' + '\n';
str += '    <h1 align="center">An Ellipse and a Rectangle</h1>' + '\n';
str += '    <svg xmlns="http://www.w3.org/Graphics/SVG/SVG-19991203.dtd" ' + '\n';
str += '         width="12cm" height="10cm">' + '\n';
str += '      <ellipse rx="110" ry="130" />' + '\n';
str += '      <rect x="4cm" y="1cm" width="3cm" height="6cm" />' + '\n';
str += '    </svg>' + '\n';
str += '    <p xlink:type="simple" xlink:href="ellipses.html">' + '\n';
str += '      More about ellipses' + '\n';
str += '    </p>' + '\n';
str += '    <p xlink:type="simple" xlink:href="rectangles.html">' + '\n';
str += '      More about rectangles' + '\n';
str += '    </p>' + '\n';
str += '    <hr/>' + '\n';
str += '    <p>Last Modified February 13, 2000</p>    ' + '\n';
str += '  </body>' + '\n';
str += '</html>';


// the regexps are built in pieces  - 
NameStrt = "[A-Za-z_:]|[^\\x00-\\x7F]";
NameChar = "[A-Za-z0-9_:.-]|[^\\x00-\\x7F]";
Name = "(" + NameStrt + ")(" + NameChar + ")*";
TextSE = "[^<]+";
UntilHyphen = "[^-]*-";
Until2Hyphens = UntilHyphen + "([^-]" + UntilHyphen + ")*-";
CommentCE = Until2Hyphens + ">?";
UntilRSBs = "[^]]*]([^]]+])*]+";
CDATA_CE = UntilRSBs + "([^]>]" + UntilRSBs + ")*>";
S = "[ \\n\\t\\r]+";
QuoteSE = '"[^"]' + "*" + '"' + "|'[^']*'";
DT_IdentSE = S + Name + "(" + S + "(" + Name + "|" + QuoteSE + "))*";
MarkupDeclCE = "([^]\"'><]+|" + QuoteSE + ")*>";
S1 = "[\\n\\r\\t ]";
UntilQMs = "[^?]*\\?+";
PI_Tail = "\\?>|" + S1 + UntilQMs + "([^>?]" + UntilQMs + ")*>";
DT_ItemSE = "<(!(--" + Until2Hyphens + ">|[^-]" + MarkupDeclCE + ")|\\?" + Name + "(" + PI_Tail + "))|%" + Name + ";|" + S;
DocTypeCE = DT_IdentSE + "(" + S + ")?(\\[(" + DT_ItemSE + ")*](" + S + ")?)?>?";
DeclCE = "--(" + CommentCE + ")?|\\[CDATA\\[(" + CDATA_CE + ")?|DOCTYPE(" + DocTypeCE + ")?";
PI_CE = Name + "(" + PI_Tail + ")?";
EndTagCE = Name + "(" + S + ")?>?";
AttValSE = '"[^<"]' + "*" + '"' + "|'[^<']*'";
ElemTagCE = Name + "(" + S + Name + "(" + S + ")?=(" + S + ")?(" + AttValSE + "))*(" + S + ")?/?>?";
MarkupSPE = "<(!(" + DeclCE + ")?|\\?(" + PI_CE + ")?|/(" + EndTagCE + ")?|(" + ElemTagCE + ")?)";
XML_SPE = TextSE + "|" + MarkupSPE;
CommentRE = "<!--" + Until2Hyphens + ">";
CommentSPE = "<!--(" + CommentCE + ")?";
PI_RE = "<\\?" + Name + "(" + PI_Tail + ")";
Erroneous_PI_SE = "<\\?[^?]*(\\?[^>]+)*\\?>";
PI_SPE = "<\\?(" + PI_CE + ")?";
CDATA_RE = "<!\\[CDATA\\[" + CDATA_CE;
CDATA_SPE = "<!\\[CDATA\\[(" + CDATA_CE + ")?";
ElemTagSE = "<(" + NameStrt + ")([^<>\"']+|" + AttValSE + ")*>";
ElemTagRE = "<" + Name + "(" + S + Name + "(" + S + ")?=(" + S + ")?(" + AttValSE + "))*(" + S + ")?/?>";
ElemTagSPE = "<" + ElemTagCE;
EndTagRE = "</" + Name + "(" + S + ")?>";
EndTagSPE = "</(" + EndTagCE + ")?";
DocTypeSPE = "<!DOCTYPE(" + DocTypeCE + ")?";
PERef_APE = "%(" + Name + ";?)?";
HexPart = "x([0-9a-fA-F]+;?)?";
NumPart = "#([0-9]+;?|" + HexPart + ")?";
CGRef_APE = "&(" + Name + ";?|" + NumPart + ")?";
Text_PE = CGRef_APE + "|[^&]+";
EntityValue_PE = CGRef_APE + "|" + PERef_APE + "|[^%&]+";
REstrings = new Array(AttValSE, CDATA_CE, CDATA_RE, CDATA_SPE, CGRef_APE, CommentCE, CommentRE, CommentSPE, DT_IdentSE, DT_ItemSE, DeclCE, DocTypeCE, DocTypeSPE, ElemTagCE, ElemTagRE, ElemTagSE, ElemTagSPE, EndTagCE, EndTagRE, EndTagSPE, EntityValue_PE, Erroneous_PI_SE, HexPart, MarkupDeclCE, MarkupSPE, Name, NameChar, NameStrt, NumPart, PERef_APE, PI_CE, PI_RE, PI_SPE, PI_Tail, QuoteSE, S, S1, TextSE, Text_PE, Until2Hyphens, UntilHyphen, UntilQMs, UntilRSBs, XML_SPE);
REnames = new Array('AttValSE', 'CDATA_CE', 'CDATA_RE', 'CDATA_SPE', 'CGRef_APE', 'CommentCE', 'CommentRE', 'CommentSPE', 'DT_IdentSE', 'DT_ItemSE', 'DeclCE', 'DocTypeCE', 'DocTypeSPE', 'ElemTagCE', 'ElemTagRE', 'ElemTagSE', 'ElemTagSPE', 'EndTagCE', 'EndTagRE', 'EndTagSPE', 'EntityValue_PE', 'Erroneous_PI_SE', 'HexPart', 'MarkupDeclCE', 'MarkupSPE', 'Name', 'NameChar', 'NameStrt', 'NumPart', 'PERef_APE', 'PI_CE', 'PI_RE', 'PI_SPE', 'PI_Tail', 'QuoteSE', 'S', 'S1', 'TextSE', 'Text_PE', 'Until2Hyphens', 'UntilHyphen', 'UntilQMs', 'UntilRSBs', 'XML_SPE');



// the reported crash happened on RegExp #24 - the 'MarkupSPE' regexp
status = inSection(1);
var re = new RegExp(REstrings[24]);
re.exec(str);
actual = RegExp.lastMatch;
expect = '<html xmlns="http://www.w3.org/1999/xhtml"' + '\n' + 
         '      xmlns:xlink="http://www.w3.org/XML/XLink/0.9">';
addThis();


// These two steps were also part of the report. Just make sure we don't crash.
var rc = RegExp.rightContext; 
re.exec(rc);



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
