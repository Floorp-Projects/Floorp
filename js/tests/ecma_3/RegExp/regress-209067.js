/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is Netscape Communications Corp.
* Portions created by the Initial Developer are Copyright (C) 2003
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK *****
*
*
* Date:    12 June 2003
* SUMMARY: Testing complicated str.replace()
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=209067
*
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 209067;
var summary = 'Testing complicated str.replace()';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function formatHTML(h)
{
  // a replace function used in the succeeding lines -
  function S(s)
  {
    return s.replace(/</g,'&lt;').replace(/>/g,'&gt;');
  }

  h+='\n';h=h.replace(/&([^\s]+;)/g,'&lt;&amp;$1&gt;');
  h=h.replace(new RegExp('<!-'+'-[\\s\\S]*-'+'->','g'), S);
  h=h.replace(/"[^"]*"/g,S);
  h=h.replace(/'[^']*'/g,S);


  h=h.replace(/<([^>]*)>/g,
              function(s,p)
              {
                if(s.match(/!doctype/i))
                  return'<span class=doctype>&lt;' + p + '&gt;</span>';
                p=p.replace(/\\'/g,'\\&#39;').replace(/\\"/g,'\\&#34;').replace(/^\s/,'');
                p=p.replace(/(\s)([^<]+)$/g,
                            function(s,p1,p2)
                            {
                              p2=p2.replace(/(=)(\s*[^"'][^\s]*)(\s|$)/g,'$1<span class=attribute-value>$2</span>$3');
                              p2=p2.replace(/("[^"]*")/g,'<span class=attribute-value>$1</span>');
                              p2=p2.replace(/('[^']*')/g,'<span class=attribute-value>$1</span>');
                              return p1 + '<span class=attribute-name>'+p2+'</span>';
                            }
                           )

                 return'&lt;<span class=' + (s.match(/<\s*\//)?'end-tag':'start-tag') + '>' + p + '</span>&gt;';
              }
             )


  h=h.replace(/&lt;(&[^\s]+;)&gt;/g,'<span class=entity>$1</span>');
  h=h.replace(/(&lt;!--[\s\S]*--&gt;)/g,'<span class=comment>$1</span>');


  numer=1;
  h=h.replace(/(.*\n)/g,
              function(s,p)
              {
                return (numer++) +'. ' + p;
              }
             )


  return'<span class=text>' + h + '</span>';
}



// sanity check
status = inSection(1);
actual = formatHTML('abc');
expect = '<span class=text>1. abc\n</span>';
addThis();


/*
 * The real test: can we run this without crashing?
 * We are not validating the result, just running it.
 */
status = inSection(2);
var HUGE_TEST_STRING = createHUGE_TEST_STRING();
formatHTML(HUGE_TEST_STRING);




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
  enterFunc('test');
  printBugNumber(bug);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


function createHUGE_TEST_STRING()
{
var HUGE_TEST_STRING = '';

HUGE_TEST_STRING += '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">';
HUGE_TEST_STRING += '<html lang="en">';
HUGE_TEST_STRING += '<head>';
HUGE_TEST_STRING += '	<meta http-equiv="content-type" content="text/html; charset=iso-8859-1">';
HUGE_TEST_STRING += '	<meta http-equiv="refresh" content="1800">';
HUGE_TEST_STRING += '	<title>CNN.com</title>';
HUGE_TEST_STRING += '	<link rel="Start" href="/">';
HUGE_TEST_STRING += '	<link rel="Search" href="/search/">';
HUGE_TEST_STRING += '	<link rel="stylesheet" href="http://i.cnn.net/cnn/.element/ssi/css/1.0/main.css" type="text/css">';
HUGE_TEST_STRING += '	<script language="JavaScript1.2" src="http://i.cnn.net/cnn/.element/ssi/js/1.0/main.js" type="text/javascript"></script>';
HUGE_TEST_STRING += '<script language="JavaScript1.1" src="http://ar.atwola.com/file/adsWrapper.js"></script>';
HUGE_TEST_STRING += '<style type="text/css">';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += '.aoltextad { text-align: justify; font-size: 12px; color: black; font-family: Georgia, sans-serif }';
HUGE_TEST_STRING += '-->';
HUGE_TEST_STRING += '</style>';
HUGE_TEST_STRING += '<script language="JavaScript1.1" type="text/javascript" src="http://ar.atwola.com/file/adsPopup2.js"></script>';
HUGE_TEST_STRING += '<script language="JavaScript">';
HUGE_TEST_STRING += 'document.adoffset = 0;';
HUGE_TEST_STRING += 'document.adPopupDomain = "www.cnn.com";';
HUGE_TEST_STRING += 'document.adPopupFile = "/cnn_adspaces/adsPopup2.html";';
HUGE_TEST_STRING += 'document.adPopupInterval = "P24";';
HUGE_TEST_STRING += 'document.adPopunderInterval = "P24";';
HUGE_TEST_STRING += 'adSetOther("&TVAR="+escape("class=us.low"));';
HUGE_TEST_STRING += '</script>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '</head>';
HUGE_TEST_STRING += '<body class="cnnMainPage">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<a name="top_of_page"></a>';
HUGE_TEST_STRING += '<a href="#ContentArea"><img src="http://i.cnn.net/cnn/images/1.gif" alt="Click here to skip to main content." width="10" height="1" border="0" align="right"></a>';
HUGE_TEST_STRING += '<table width="770" border="0" cellpadding="0" cellspacing="0" style="speak: none">';
HUGE_TEST_STRING += '	<col width="229">';
HUGE_TEST_STRING += '	<col width="73">';
HUGE_TEST_STRING += '	<col width="468">';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td colspan="3"><!--';
HUGE_TEST_STRING += '[[!~~ netscape hat ~~]][[table border="0" cellpadding="0" cellspacing="0" width="100%"]][[tr]][[td]][[script Language="Javascript" SRC="http://toolbar.aol.com/dashboard.twhat?dom=cnn" type="text/javascript"]][[/script]][[/td]][[/tr]][[/table]]';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '[[div]][[img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="2" border="0"]][[/div]]';
HUGE_TEST_STRING += '-->';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr valign="bottom">';
HUGE_TEST_STRING += '		<td width="229" style="speak: normal"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/logo/cnn.gif" alt="CNN.com" width="229" height="52" border="0"></td>';
HUGE_TEST_STRING += '		<td width="73"></td>';
HUGE_TEST_STRING += '		<td width="468" align="right">';
HUGE_TEST_STRING += '			<!-- home/bottom.468x60 -->';
HUGE_TEST_STRING += '<script language="JavaScript1.1">';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += 'adSetTarget("_top");';
HUGE_TEST_STRING += 'htmlAdWH( (new Array(93103287,93103287,93103300,93103300))[document.adoffset||0] , 468, 60);';
HUGE_TEST_STRING += '//-->';
HUGE_TEST_STRING += '</script>';
HUGE_TEST_STRING += '<noscript><a href="http://ar.atwola.com/link/93103287/aol" target="_top"><img src="http://ar.atwola.com/image/93103287/aol" alt="Click Here" width="468" height="60" border="0"></a></noscript> ';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr><td colspan="3"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="2"></td></tr>';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td colspan="3">';
HUGE_TEST_STRING += '</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr><td colspan="3" bgcolor="#CC0000"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="3"></td></tr>';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td colspan="3">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="770" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<form action="http://search.cnn.com/cnn/search" method="get" onsubmit="return CNN_validateSearchForm(this);">';
HUGE_TEST_STRING += '<input type="hidden" name="source" value="cnn">';
HUGE_TEST_STRING += '<input type="hidden" name="invocationType" value="search/top">';
HUGE_TEST_STRING += '	<tr><td colspan="4"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1" border="0"></td></tr>';
HUGE_TEST_STRING += '	<tr><td colspan="4" bgcolor="#003366"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="3" border="0"></td></tr>';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td rowspan="2"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/searchbar/bar.search.gif" alt="SEARCH" width="110" height="27" border="0"></td>';
HUGE_TEST_STRING += '		<td colspan="2"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/searchbar/bar.top.bevel.gif" alt="" width="653" height="3" border="0"></td>';
HUGE_TEST_STRING += '		<td rowspan="2"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/searchbar/bar.right.bevel.gif" alt="" width="7" height="27" border="0"></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr bgcolor="#B6D8E0">';
HUGE_TEST_STRING += '		<td><table border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '				<tr>';
HUGE_TEST_STRING += '					<td>&nbsp;&nbsp;</td>';
HUGE_TEST_STRING += '					<td nowrap><span class="cnnFormTextB" style="color:#369">The Web</span></td>';
HUGE_TEST_STRING += '					<td><input type="radio" name="sites" value="google" checked></td>';
HUGE_TEST_STRING += '					<td>&nbsp;&nbsp;</td>';
HUGE_TEST_STRING += '					<td><span class="cnnFormTextB" style="color:#369;">CNN.com</span></td>';
HUGE_TEST_STRING += '					<td><input type="radio" name="sites" value="cnn"></td>';
HUGE_TEST_STRING += '					<td>&nbsp;&nbsp;</td>';
HUGE_TEST_STRING += '					<td><input type="text" name="query" class="cnnFormText" value="" title="Enter text to search for and click Search" size="35" maxlength="40" style="width: 280px"></td>';
HUGE_TEST_STRING += '					<td>&nbsp;<input type="Submit" value="Search" class="cnnNavButton" style="padding: 0px; margin: 0px; width: 50px"></td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table></td>';
HUGE_TEST_STRING += '		<td align="right"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/searchbar/bar.google.gif" alt="enhanced by Google" width="137" height="24" border="0"></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr><td colspan="4"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/searchbar/bar.bottom.bevel.gif" alt="" width="770" height="3" border="0"></td></tr>';
HUGE_TEST_STRING += '	</form>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="770" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<col width="126" align="left" valign="top">';
HUGE_TEST_STRING += '	<col width="10">';
HUGE_TEST_STRING += '	<col width="280">';
HUGE_TEST_STRING += '	<col width="10">';
HUGE_TEST_STRING += '	<col width="344">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td rowspan="5" width="126" style="speak: none"><table id="cnnNavBar" width="126" bgcolor="#EEEEEE" border="0" cellpadding="0" cellspacing="0" summary="CNN.com Navigation">';
HUGE_TEST_STRING += '	<col width="8" align="left" valign="top">';
HUGE_TEST_STRING += '	<col width="118" align="left" valign="top">';
HUGE_TEST_STRING += '	<tr bgcolor="#CCCCCC" class="cnnNavHiliteRow"><td width="8" class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNavHilite" onClick="CNN_goTo("/")"><div class="cnnNavText"><a href="/">Home Page</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/WORLD/")"><div class="cnnNavText"><a href="/WORLD/">World</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/US/")"><div class="cnnNavText"><a href="/US/">U.S.</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/WEATHER/")"><div class="cnnNavText"><a href="/WEATHER/">Weather</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/money/")"><div class="cnnNavText"><a href="/money/">Business</a>&nbsp;<a href="/money/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/nav_at_money.gif" alt="at CNN/Money" width="51" height="5" border="0"></a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/cnnsi/")"><div class="cnnNavText"><a href="/si/">Sports</a>&nbsp;<a href="/si/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/nav_at_si.gif" alt="at SI.com" width="50" height="5" border="0"></a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/ALLPOLITICS/")"><div class="cnnNavText"><a href="/ALLPOLITICS/">Politics</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/LAW/")"><div class="cnnNavText"><a href="/LAW/">Law</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/TECH/")"><div class="cnnNavText"><a href="/TECH/">Technology</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/TECH/space/")"><div class="cnnNavText"><a href="/TECH/space/">Science &amp; Space</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/HEALTH/")"><div class="cnnNavText"><a href="/HEALTH/">Health</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/SHOWBIZ/")"><div class="cnnNavText"><a href="/SHOWBIZ/">Entertainment</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/TRAVEL/")"><div class="cnnNavText"><a href="/TRAVEL/">Travel</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/EDUCATION/")"><div class="cnnNavText"><a href="/EDUCATION/">Education</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNav" onMouseOver="CNN_navBar(this,1,1)" onMouseOut="CNN_navBar(this,0,1)" onClick="CNN_navBarClick(this,1,"/SPECIALS/")"><div class="cnnNavText"><a href="/SPECIALS/">Special Reports</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr bgcolor="#FFFFFF"><td class="cnnNavAd" colspan="2" align="center"><!-- home/left.120x90 -->';
HUGE_TEST_STRING += '<script language="JavaScript1.1">';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += 'adSetTarget("_top");';
HUGE_TEST_STRING += 'htmlAdWH( (new Array(93166917,93166917,93170132,93170132))[document.adoffset||0] , 120, 90);';
HUGE_TEST_STRING += '//-->';
HUGE_TEST_STRING += '</script><noscript><a href="http://ar.atwola.com/link/93166917/aol" target="_top"><img src="http://ar.atwola.com/image/93166917/aol" alt="Click here for our advertiser" width="120" height="90" border="0"></a></noscript></td></tr>';
HUGE_TEST_STRING += '	<tr bgcolor="#999999" class="cnnNavGroupRow">';
HUGE_TEST_STRING += '		<td colspan="2" class="cnnNavGroup"><div class="cnnNavText">SERVICES</div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavOtherRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNavOther" onMouseOver="CNN_navBar(this,1,0)" onMouseOut="CNN_navBar(this,0,0)" onClick="CNN_navBarClick(this,0,"/video/")"><div class="cnnNavText"><a href="/video/">Video</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavOtherRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNavOther" onMouseOver="CNN_navBar(this,1,0)" onMouseOut="CNN_navBar(this,0,0)" onClick="CNN_navBarClick(this,0,"/EMAIL/")"><div class="cnnNavText"><a href="/EMAIL/">E-Mail Services</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr class="cnnNavOtherRow"><td class="swath">&nbsp;</td>';
HUGE_TEST_STRING += '		<td class="cnnNavOther" onMouseOver="CNN_navBar(this,1,0)" onMouseOut="CNN_navBar(this,0,0)" onClick="CNN_navBarClick(this,0,"/mobile/CNNtoGO/")"><div class="cnnNavText"><a href="/mobile/CNNtoGO/">CNN To Go</a></div></td></tr>';
HUGE_TEST_STRING += '	<tr bgcolor="#999999" class="cnnNavGroupRow">';
HUGE_TEST_STRING += '		<td colspan="2" class="cnnNavGroup" style="background-color: #445B60"><div class="cnnNavText" style="color: #fff">SEARCH</div></td></tr>';
HUGE_TEST_STRING += '	<tr bgcolor="#CCCCCC"><td colspan="2" class="cnnNavSearch" style="background-color:#B6D8E0">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<form action="http://search.cnn.com/cnn/search" method="get" name="nav_bottom_search" onSubmit="return CNN_validateSearchForm(this)" style="margin: 0px;">';
HUGE_TEST_STRING += '	<input type="hidden" name="sites" value="cnn">';
HUGE_TEST_STRING += '	<input type="hidden" name="source" value="cnn">';
HUGE_TEST_STRING += '	<input type="hidden" name="invocationType" value="side/bottom">';
HUGE_TEST_STRING += '<table width="100%" border="0" cellpadding="0" cellspacing="4">';
HUGE_TEST_STRING += '	<tr><td colspan="2"><table width="100%" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '			<tr>';
HUGE_TEST_STRING += '				<td align="left"><span class="cnnFormTextB" style="color: #369">Web</span></td>';
HUGE_TEST_STRING += '				<td><input type="radio" name="sites" value="google" checked></td>';
HUGE_TEST_STRING += '				<td align="right"><span class="cnnFormTextB" style="color: #369">CNN.com</span></td>';
HUGE_TEST_STRING += '				<td><input type="radio" name="sites" value="cnn"></td>';
HUGE_TEST_STRING += '			</tr>';
HUGE_TEST_STRING += '		</table></td></tr>';
HUGE_TEST_STRING += '	<tr><td colspan="2"><input type="text" name="query" class="cnnFormText" value="" title="Enter text to search for and click Search" size="7" maxlength="40" style="width: 100%"></td></tr>';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td><input type="submit" value="Search" class="cnnNavButton" style="padding: 0px; margin: 0px; width: 50px"></td>';
HUGE_TEST_STRING += '		<td align="right"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/sect/SEARCH/nav.search.gif" alt="enhanced by Google" width="54" height="27"></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '</td></form></tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '		<td rowspan="5" width="10"><a name="ContentArea"></a><img id="accessibilityPixel" src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="7" border="0"></td>';
HUGE_TEST_STRING += '		<td colspan="3" valign="middle">';
HUGE_TEST_STRING += '			<table border="0" cellpadding="0" cellspacing="0" width="100%">';
HUGE_TEST_STRING += '				<tr>';
HUGE_TEST_STRING += '					<td valign="top" nowrap><div class="cnnFinePrint" style="color: #333;padding:6px;padding-left:0px;">Updated: 05:53 p.m. EDT (2153 GMT) June 12, 2003</div></td>';
HUGE_TEST_STRING += '					<td align="right" nowrap class="cnnt1link"><a href="http://edition.cnn.com/">Visit International Edition</a>&nbsp;</td>';
HUGE_TEST_STRING += '				</tr><!--include virtual="/.element/ssi/sect/MAIN/1.0/banner.html"-->';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td rowspan="2" width="280" bgcolor="#EAEFF4">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- T1 -->';
HUGE_TEST_STRING += '					';
HUGE_TEST_STRING += '					<a href="/2003/SHOWBIZ/Movies/06/12/obit.peck/index.html"><img src="http://i.cnn.net/cnn/2003/SHOWBIZ/Movies/06/12/obit.peck/top.peck.obit.jpg" alt="Oscar-winner Peck dies" width="280" height="210" border="0" hspace="0" vspace="0"></a>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '						<div class="cnnMainT1">';
HUGE_TEST_STRING += '		<h2 style="font-size:20px;"><a href="/2003/SHOWBIZ/Movies/06/12/obit.peck/index.html">Oscar-winner Peck dies</a></h2>';
HUGE_TEST_STRING += '<p>';
HUGE_TEST_STRING += 'Actor Gregory Peck, who won an Oscar for his portrayal of upstanding lawyer Atticus Finch in 1962s "To Kill a Mockingbird," has died at age 87. Peck was best known for roles of dignified statesmen and people who followed a strong code of ethics. But he also could play against type. All told, Peck was nominated for five Academy Awards.';
HUGE_TEST_STRING += '</p>';
HUGE_TEST_STRING += '		<p>';
HUGE_TEST_STRING += '			<b><a href="/2003/SHOWBIZ/Movies/06/12/obit.peck/index.html" class="cnnt1link">FULL STORY</a></b>';
HUGE_TEST_STRING += '		</p>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '&#8226; <span class="cnnBodyText" style="font-weight:bold;color:#333;">Video: </span><img src="http://i.cnn.net/cnn/.element/img/1.0/misc/premium.gif" alt="premium content" width="9" height="11" hspace="0" vspace="0" border="0" align="absmiddle">  <a href="javascript:LaunchVideo("/showbiz/2003/06/12/peck.obit.affl.","300k");">A leading mans leading man</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '		';
HUGE_TEST_STRING += '&#8226; <span class="cnnBodyText" style="font-weight:bold;color:#333">Interactive: </span> <a href="javascript:CNN_openPopup("/interactive/entertainment/0306/peck.obit/frameset.exclude.html","620x430","toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=620,height=430")">Gregory Peck through the years</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;  <a href="http://www.cnn.com/2003/SHOWBIZ/Movies/06/12/peck.filmography/index.html" target="new">Gregory Peck filmography</a><img src="http://i.cnn.net/cnn/.element/img/1.0/misc/icon.external.links.gif" alt="external link" width="20" height="13" vspace="1" hspace="4" border="0" align="top"><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;  <a href="http://www.cnn.com/2003/SHOWBIZ/Movies/06/04/heroes.villains.ap/index.html" target="new">Pecks Finch chararcter AFIs top hero</a><img src="http://i.cnn.net/cnn/.element/img/1.0/misc/icon.external.links.gif" alt="external link" width="20" height="13" vspace="1" hspace="4" border="0" align="top"><br>';
HUGE_TEST_STRING += '	</div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- /T1 -->';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '		';
HUGE_TEST_STRING += '		<td rowspan="2" width="10"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="10" height="1"></td>';
HUGE_TEST_STRING += '		<td width="344">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- T2 -->';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="344" height="2"></div>';
HUGE_TEST_STRING += '<table width="344" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td width="285" class="cnnTabbedBoxHeader" style="padding-left:0px;"><span class="cnnBigPrint"><b>MORE TOP STORIES</b></span></td>';
HUGE_TEST_STRING += ' 		<td width="59" class="cnnTabbedBoxTab" align="right" bgcolor="#336699"><a href="/userpicks"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/userpicks.gif" alt=" Hot Stories " width="59" height="11" border="0"></a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '<div style="padding:6px;padding-left:0px;">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/WORLD/meast/06/12/mideast/index.html">7 dead in new Gaza strike</a>';
HUGE_TEST_STRING += '| <img src="http://i.cnn.net/cnn/.element/img/1.0/misc/premium.gif" alt="premium content" width="9" height="11" hspace="0" vspace="0" border="0" align="absmiddle"> <a href="javascript:LaunchVideo("/world/2003/06/11/cb.bush.roadmap.ap.","300k");">Video</a><br></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/WORLD/meast/06/12/sprj.irq.main/index.html">U.S. helicopter, jet down in Iraqi raid</a>';
HUGE_TEST_STRING += '| <img src="http://i.cnn.net/cnn/.element/img/1.0/misc/premium.gif" alt="premium content" width="9" height="11" hspace="0" vspace="0" border="0" align="absmiddle"> <a href="javascript:LaunchVideo("/iraq/2003/06/11/bw.iraq.oil.cnn.","300k");">Video</a><br></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/SHOWBIZ/TV/06/12/obit.brinkley/index.html">Television icon David Brinkley dead at 82</a><br></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/LAW/06/12/peterson.case/index.html">Peterson search warrants will be made public in July</a><br></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/WORLD/asiapcf/east/06/12/okinawa.rape/index.html">U.S. Marine held in new Okinawa rape case</a><br></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/TECH/space/06/12/sprj.colu.bolts.ap/index.html">New threat discovered for shuttle launches</a><br></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<div class="cnnMainNewT2">&#8226; <a href="/2003/SHOWBIZ/TV/06/12/television.sopranos.reut/index.html">"Soprano" Gandolfini shares his wealth with castmates</a><br></div>';
HUGE_TEST_STRING += '<!--[[div class="cnnMainNewT2"]]&#8226;&nbsp;[[b]][[span style="color:#C00;"]]CNN[[/span]]Radio:[[/b]]&nbsp;[[a href="javascript:CNN_openPopup("/audio/radio/preferences.html","radioplayer","toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=200,height=124")"]]Bush on Medicare[[/a]]&nbsp;[[a href="javascript:CNN_openPopup("/audio/radio/preferences.html","radioplayer","toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=200,height=124")"]][[img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/live.video.gif" alt="" width="61" height="14" vspace="0" hspace="2" align="absmiddle" border="0"]][[/a]][[img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/audio.gif" alt="" width="10" height="10" vspace="0" hspace="2" align="absmiddle"]][[br]][[/div]]--></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- /T2 -->';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!--include virtual="/.element/ssi/misc/1.0/war.zone.smmap.txt"-->';
HUGE_TEST_STRING += '<!-- =========== CNN Radio/Video Box =========== -->';
HUGE_TEST_STRING += '<!-- top line -->	';
HUGE_TEST_STRING += '<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_ccc.gif" alt="" width="344" height="1"></div>';
HUGE_TEST_STRING += '<!-- /top line -->';
HUGE_TEST_STRING += ' <table width="344" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '<!-- left-side line -->	';
HUGE_TEST_STRING += '		<td bgcolor="#CCCCCC" width="1"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="30" hspace="0" vspace="0" border="0"></td>';
HUGE_TEST_STRING += '<!-- /left-side line -->	';
HUGE_TEST_STRING += '<!-- CNNRadio cell -->';
HUGE_TEST_STRING += '        <td width="114"><div class="cnn6pxPad">';
HUGE_TEST_STRING += '        <span class="cnnBigPrint" style="color:#C00;font-weight:bold;">CNN</span><span class="cnnBigPrint" style="color:#000;font-weight:bold;">RADIO</span>';
HUGE_TEST_STRING += '<div class="cnnMainNewT2"><a href="javascript:CNN_openPopup("/audio/radio/preferences.html","radioplayer","toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=200,height=124")">Listen to latest updates</a><img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/audio.gif" alt="" width="10" height="10" vspace="0" hspace="2" align="absmiddle">';
HUGE_TEST_STRING += '<div><img src="http://i.a.cnn.net/cnn/images/1.gif" alt="" width="1" height="5" hspace="0" vspace="0"></div>';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += '[[span class="cnnFinePrint"]]sponsored by:[[/span]][[br]][[center]]';
HUGE_TEST_STRING += '[[!~~#include virtual="/cnn_adspaces/home/war_in_iraq/sponsor.88x31.ad"~~]]';
HUGE_TEST_STRING += ' [[/center]]';
HUGE_TEST_STRING += '-->';
HUGE_TEST_STRING += ' </div></td>';
HUGE_TEST_STRING += '<!-- /CNNRadio cell --> ';
HUGE_TEST_STRING += '<!-- center line -->  ';
HUGE_TEST_STRING += '		<td bgcolor="#CCCCCC" width="1"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1" hspace="0" vspace="0" border="0"></td>';
HUGE_TEST_STRING += '<!-- /center line --> ';
HUGE_TEST_STRING += '<!-- video cell --> ';
HUGE_TEST_STRING += '       <td width="227"><div class="cnn6pxPad">';
HUGE_TEST_STRING += '<!-- video box -->       ';
HUGE_TEST_STRING += '<table width="215" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '   <tr valign="top">';
HUGE_TEST_STRING += '    <td width="144"><span class="cnnBigPrint" style="font-weight:bold;">VIDEO</span></td>';
HUGE_TEST_STRING += '    <td width="6"><img src="http://i.a.cnn.net/cnn/images/1.gif" alt="" width="6" height="1" hspace="0" vspace="0"></td>';
HUGE_TEST_STRING += '	<td width="65"><a href="/video/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/more.video.blue.gif" alt="MORE VIDEO" width="62" height="11" hspace="0" vspace="0" border="0"></a></td></tr>';
HUGE_TEST_STRING += '   <tr>';
HUGE_TEST_STRING += '    <td width="215" colspan="3"><img src="http://i.a.cnn.net/cnn/images/1.gif" alt="" width="1" height="2" hspace="0" vspace="0"></td></tr>';
HUGE_TEST_STRING += '  <tr valign="top">';
HUGE_TEST_STRING += '    <td><div class="cnnBodyText">';
HUGE_TEST_STRING += '     	Soldier broke dozens of hearts over e-mail<br>';
HUGE_TEST_STRING += '     <img src="http://i.a.cnn.net/cnn/images/icons/premium.gif" align="middle" alt="premium content" width="9" height="11" hspace="0" vspace="1" border="0">&nbsp;<a href="javascript:LaunchVideo("/offbeat/2003/06/12/ms.casanova.col.ap.","300k");" class="cnnVideoLink">PLAY VIDEO</a></div>';
HUGE_TEST_STRING += '  </td>';
HUGE_TEST_STRING += '<td width="3"><img src="http://i.a.cnn.net/cnn/images/1.gif" alt="" width="3" height="1" hspace="0" vspace="0"></td>  ';
HUGE_TEST_STRING += '  <td width="65" align="right">';
HUGE_TEST_STRING += '    <a href="javascript:LaunchVideo("/offbeat/2003/06/12/ms.casanova.col.ap.","300k");"><img src="http://i.cnn.net/cnn/video/offbeat/2003/06/12/ms.casanova.col.vs.kndu.jpg" alt="" width="65" height="49" border="0" vspace="2" hspace="0"></a>';
HUGE_TEST_STRING += '  </td></tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += ' <!-- /video box -->        ';
HUGE_TEST_STRING += '       </div></td>';
HUGE_TEST_STRING += '<!-- /video cell -->        ';
HUGE_TEST_STRING += '<!-- right-side line -->       ';
HUGE_TEST_STRING += '<td bgcolor="#CCCCCC" width="1"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1" hspace="0" vspace="0" border="0"></td>';
HUGE_TEST_STRING += '<!-- /right-side line -->  ';
HUGE_TEST_STRING += '		</tr>';
HUGE_TEST_STRING += '  </table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- bottom line -->';
HUGE_TEST_STRING += '<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_ccc.gif" alt="" width="344" height="1"></div>';
HUGE_TEST_STRING += '<!-- /bottom line -->';
HUGE_TEST_STRING += '<!-- =========== /CNN Radio/Video Box =========== -->';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="344" height="2"></div>';
HUGE_TEST_STRING += '<table width="344" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td width="260" class="cnnTabbedBoxHeader" style="padding-left:0px;"><span class="cnnBigPrint"><b>ON THE SCENE</b></span></td>';
HUGE_TEST_STRING += '		<td width="84" class="cnnTabbedBoxTab" align="right" bgcolor="#336699" style="padding: 0px 3px;"><a href="/LAW/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/superlinks/law.gif" alt="more reports" height="11" border="0" hspace="2" vspace="2" align="right"></a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="344" border="0" cellpadding="5" cellspacing="0">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td style="padding-left:0px;">                                                                                                                                <b>Jeffrey Toobin:</b> "It takes guts" for Peterson defense to subpoena judge over wiretap issue.';
HUGE_TEST_STRING += '<a href="/2003/LAW/06/12/otsc.toobin/index.html">Full Story</a></td>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<td width="65" align="right" style="padding-left:6px;"><a href="/2003/LAW/06/12/otsc.toobin/index.html"><img src="http://i.cnn.net/cnn/2003/LAW/06/12/otsc.toobin/tz.toobin.jpg" alt="image" width="65" height="49" border="0" hspace="0" vspace="0"></a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr valign="bottom">';
HUGE_TEST_STRING += '		<td>';
HUGE_TEST_STRING += '<table width="344" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td width="267" nowrap style="color: #c00; padding-left: 6px"><span class="cnnBigPrint" style="vertical-align: top"><b>BUSINESS</b></span>';
HUGE_TEST_STRING += '			<a href="/money/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/at_cnnmoney.gif" alt=" at CNN/Money " width="100" height="15" border="0"></a></td>';
HUGE_TEST_STRING += '		<td width="77" align="right"><a href="/money/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/business.news.blue.gif" alt=" Business News " width="77" height="11" border="0"></a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="344" bgcolor="#EEEEEE" border="0" cellpadding="0" cellspacing="0" style="border: solid 1px #ddd">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td>';
HUGE_TEST_STRING += '			<table width="100%" border="0" cellpadding="0" cellspacing="4">';
HUGE_TEST_STRING += '				<tr>';
HUGE_TEST_STRING += '					<td colspan="3"><span class="cnnMenuText"><b>STOCK/FUND QUOTES: </b></span></td>';
HUGE_TEST_STRING += '				</tr><form action="http://qs.money.cnn.com/tq/stockquote" method="get" style="margin: 0px;">';
HUGE_TEST_STRING += '				<tr>';
HUGE_TEST_STRING += '					<td><span class="cnnFinePrint">enter symbol</span></td>';
HUGE_TEST_STRING += '					<td><input type="text" name="symbols" size="7" maxlength="40" class="cnnMenuText" title="Enter stock/fund symbol or name to get a quote"></td>';
HUGE_TEST_STRING += '					<td><input type="submit" value="GET" class="cnnNavButton"></td>';
HUGE_TEST_STRING += '				</tr></form>';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '			<table width="100%" border="0" cellpadding="0" cellspacing="4">';
HUGE_TEST_STRING += '				<tr valign="top">';
HUGE_TEST_STRING += '					<td><span class="cnnFinePrint">sponsored by:</span></td>';
HUGE_TEST_STRING += '					<td align="right"><!--<a href="/money/news/specials/rebuild_iraq/"><img src="http://i.a.cnn.net/cnn/2003/images/04/17/money.box.gif" ALT="" width="150" height="31" HSPACE="0" VSPACE="0" border="0" align="left"></a>--><a href="http://ar.atwola.com/link/93103306/aol"><img src="http://ar.atwola.com/image/93103306/aol" alt="Click Here" width="88" height="31" border="0" hspace="0" vspace="0"></a></td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '			</td>';
HUGE_TEST_STRING += '		<td class="cnnMainMarketBox">		<table width="100%" border="0" cellpadding="4" cellspacing="0" summary="Market data from CNNmoney">';
HUGE_TEST_STRING += '			<tr class="noBottomBorder">';
HUGE_TEST_STRING += '				<td colspan="5"><span class="cnnMainMarketCell"><span class="cnnMenuText"><b><a href="/money/markets/">MARKETS:</a></b></span> <!-- 16:30:15 -->';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '4:30pm ET, 6/12</span></td>';
HUGE_TEST_STRING += '			</tr>';
HUGE_TEST_STRING += '			<tr class="noTopBorder">';
HUGE_TEST_STRING += '				<td><span class="cnnMainMarketCell"><a href="/money/markets/dow.html" title="Dow Jones Industrial Average">DJIA</a></span></td>';
HUGE_TEST_STRING += '								<td><img src="http://i.cnn.net/cnn/.element/img/1.0/main/arrow_up.gif" alt="" width="9" height="9"></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">+13.30</span></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">9196.50</span></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">+ 0.14%</span></td>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '			</tr>';
HUGE_TEST_STRING += '			<tr>';
HUGE_TEST_STRING += '				<td><span class="cnnMainMarketCell"><a href="/money/markets/nasdaq.html" title="NASDAQ">NAS</a></span></td>';
HUGE_TEST_STRING += '								<td><img src="http://i.cnn.net/cnn/.element/img/1.0/main/arrow_up.gif" alt="" width="9" height="9"></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">+ 7.60</span></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">1653.62</span></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">+ 0.46%</span></td>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '			</tr>';
HUGE_TEST_STRING += '			<tr class="noBottomBorder">';
HUGE_TEST_STRING += '				<td><span class="cnnMainMarketCell"><a href="/money/markets/sandp.html" title="S&amp;P 500">S&amp;P</a></span></td>';
HUGE_TEST_STRING += '								<td><img src="http://i.cnn.net/cnn/.element/img/1.0/main/arrow_up.gif" alt="" width="9" height="9"></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">+ 1.03</span></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">998.51</span></td>';
HUGE_TEST_STRING += '				<td align="right" nowrap><span class="cnnMainMarketCell">+ 0.10%</span></td>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '			</tr>';
HUGE_TEST_STRING += '		</table>';
HUGE_TEST_STRING += '</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td colspan="3"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="4"></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr align="center" valign="bottom">';
HUGE_TEST_STRING += '		<td width="280" bgcolor="#EEEEEE"><a href="/linkto/ftn.nytimes1.html"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/ftn.280x32.ny.times.gif" width="255" height="32" alt="" border="0"></a></td>';
HUGE_TEST_STRING += '<td width="10"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="10" height="1"></td>';
HUGE_TEST_STRING += '		<td width="344" bgcolor="#EEEEEE"><a href="/linkto/ftn.bn3.html"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/ftn.345x32.breaking.news.gif" width="340" height="32" alt="" border="0"></a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="770" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<col width="10">';
HUGE_TEST_STRING += '	<col width="483" align="left" valign="top">';
HUGE_TEST_STRING += '	<col width="10">';
HUGE_TEST_STRING += '	<col width="267" align="left" valign="top">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td rowspan="2"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="10" height="1"></td>';
HUGE_TEST_STRING += '		<td valign="top">';
HUGE_TEST_STRING += '			<table border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '				<tr valign="top">';
HUGE_TEST_STRING += '					<td width="238">';
HUGE_TEST_STRING += '						<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="238" height="2"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '						<table width="238" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '							<tr>';
HUGE_TEST_STRING += '						<td width="132" class="cnnTabbedBoxHeader" style="padding-left:0px;"><span class="cnnBigPrint"><b>MORE REAL TV</b></span></td>';
HUGE_TEST_STRING += '						<td width="106" class="cnnTabbedBoxTab" align="right" bgcolor="#336699" style="padding: 0px 3px;"><a href="/SHOWBIZ"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/entertainment.news.gif" alt="More Entertainment" border="0" width="102" height="11" hspace="2" vspace="2" align="right"></a></td>';
HUGE_TEST_STRING += '					</tr>';
HUGE_TEST_STRING += '				</table>';
HUGE_TEST_STRING += '				<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="238" height="5" vspace="0" hspace="0"></div>';
HUGE_TEST_STRING += '						<table width="238" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '							<tr valign="top">';
HUGE_TEST_STRING += '								<td><div class="cnn6pxTpad">';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += ' <a href="/2003/SHOWBIZ/06/11/eye.ent.voyeurs/index.html">Go ahead, follow me</a><br>';
HUGE_TEST_STRING += 'New reality series and the movie debut of "Idol" finalists';
HUGE_TEST_STRING += '								</div></td>';
HUGE_TEST_STRING += '								<td width="71" align="right"><a href="/2003/SHOWBIZ/06/11/eye.ent.voyeurs/index.html"><img src="http://i.a.cnn.net/cnn/2003/SHOWBIZ/06/11/eye.ent.voyeurs/tz.movies.gif" alt="Go ahead, follow me" width="65" height="49" border="0" vspace="6"></a></td>';
HUGE_TEST_STRING += '							</tr>';
HUGE_TEST_STRING += '						</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '			';
HUGE_TEST_STRING += '				<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="238" height="5" vspace="0" hspace="0"></div>';
HUGE_TEST_STRING += '<!--include virtual="/.element/ssi/video/section_teases/topvideos_include.txt"-->';
HUGE_TEST_STRING += '					</td>';
HUGE_TEST_STRING += '					<td><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="7" height="1"></td>';
HUGE_TEST_STRING += '					<td width="238">';
HUGE_TEST_STRING += '						<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="238" height="2"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '						<table width="238" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '							<tr>';
HUGE_TEST_STRING += '						<td width="157" class="cnnTabbedBoxHeader" style="padding-left:0px;"><span class="cnnBigPrint"><b>GIFT IDEAS</b></span></td>';
HUGE_TEST_STRING += '						<td width="81" class="cnnTabbedBoxTab" align="right" bgcolor="#336699" style="padding: 0px 3px;"><a href="/money"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/superlinks/business.gif" alt="Business News" border="0" width="77" height="11" hspace="2" vspace="2" align="right"></a></td>';
HUGE_TEST_STRING += '					</tr>';
HUGE_TEST_STRING += '				</table>';
HUGE_TEST_STRING += '				<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="238" height="5" vspace="0" hspace="0"></div>';
HUGE_TEST_STRING += '						<table width="238" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '							<tr valign="top">';
HUGE_TEST_STRING += '								<td><div class="cnn6pxTpad">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<span class="cnnBodyText" style="font-weight:bold;">CNN/Money: </span> <a href="/money/2003/06/12/news/companies/fathers_day/index.htm?cnn=yes">Fathers Day</a><br>';
HUGE_TEST_STRING += 'Smaller is better --from digital cameras to iPod';
HUGE_TEST_STRING += '								</div></td>';
HUGE_TEST_STRING += '								<td width="71" align="right"><a href="/money/2003/06/12/news/companies/fathers_day/index.htm?cnn=yes"><img src="http://i.a.cnn.net/cnn/images/programming.boxes/tz.money.dads.day.watch.jpg" alt="Fathers Day" width="65" height="49" border="0" vspace="6"></a></td>';
HUGE_TEST_STRING += '							</tr>';
HUGE_TEST_STRING += '						</table>';
HUGE_TEST_STRING += '					</td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '				<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="238" height="10" vspace="0" hspace="0"></div>			';
HUGE_TEST_STRING += '<table width="483" border="0" cellspacing="0" cellpadding="0">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td rowspan="9"><br></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/US/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/us.gif" alt="U.S. News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/US/South/06/11/miami.rapist/index.html">Miami police link 4 rapes to serial rapist</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/LAW/06/12/mistaken.identity.ap/index.html">Woman mistaken for fugitive jailed</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/US/Northeast/06/12/woman.impaled.ap/index.html">Pregnant woman impaled on mic stand</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td rowspan="7" width="7"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="7" height="1"></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/WORLD/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/world.gif" alt="World News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/WORLD/europe/06/12/nato.bases/index.html">NATO reshapes for new era</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/WORLD/africa/06/12/congo.democratic/index.html">U.N. reviews Bunia peace force</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '&#8226;&nbsp;<span class="cnnBodyText" style="font-weight:bold;color:#900;">TIME.com: </span><a href="/time/magazine/article/0,9171,1101030616-457361,00.html?CNN=yes" target="new">Saddams curtain trail</a><img src="http://i.cnn.net/cnn/.element/img/1.0/misc/icon.external.links.gif" alt="external link" width="20" height="13" vspace="1" hspace="4" border="0" align="top"><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '	</tr><tr valign="top">';
HUGE_TEST_STRING += '		<td width="238"><a href="/TECH/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/technology.gif" alt="Sci-Tech News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/TECH/ptech/06/11/bus2.ptech.dvd.maker/index.html">Another reason to throw out your VCR</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/TECH/ptech/06/12/korea.samsung.reut/index.html">Flat screen TV prices dropping</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/SHOWBIZ/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/entertainment.gif" alt="Entertainment News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/SHOWBIZ/TV/06/12/cnn.obrien/index.html">CNN hires Soledad OBrien for "AM"</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/SHOWBIZ/TV/06/11/batchelor.troubles.ap/index.html">Dating show star let go by law firm</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '	</tr><tr valign="top">';
HUGE_TEST_STRING += '		<td width="238"><a href="/ALLPOLITICS/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/politics.gif" alt="Politics News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/ALLPOLITICS/06/11/schwarzenegger.ap/index.html">Schwarzenegger on California politics</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/ALLPOLITICS/06/12/tax.credit.ap/index.html">House approves extension on child tax credit</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/LAW/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/law.gif" alt="Law News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/LAW/06/12/plaintiff.advances.ap/index.html">Court bars cash advances to plaintiffs</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/LAW/06/11/jackson.lawsuit.ap/index.html">Lawsuit against Jackson settled</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '	</tr><tr valign="top">';
HUGE_TEST_STRING += '		<td width="238"><a href="/HEALTH/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/health.gif" alt="Health News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/HEALTH/06/12/monkeypox.ap/index.html">Monkeypox spreading person-to-person?</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/HEALTH/06/12/quick.xray.ap/index.html">A full body X-ray in 13 seconds</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/TECH/space/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/space.gif" alt="Space News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/TECH/science/06/12/hydrogen.ozone.ap/index.html">Hydrogen fuel may disturb ozone layer</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/TECH/space/06/12/sprj.colu.bolts.ap/index.html">New threat found for shuttle launches</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '	</tr><tr valign="top">';
HUGE_TEST_STRING += '		<td width="238"><a href="/TRAVEL/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/travel.gif" alt="Travel News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/TRAVEL/DESTINATIONS/06/12/walk.across.america.ap/index.html">Walking America from coast to coast</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/TRAVEL/06/11/bi.airlines.executives.reut/index.html">Airline execs not seeing sunny skies yet</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/EDUCATION/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/education.gif" alt="Education News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/EDUCATION/06/12/arabs.prom.ap/index.html">Arab students seek prom balance</a><br>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/2003/EDUCATION/06/11/school.fundraising.ap/index.html">Public schools turn to upscale fundraising</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '	</tr><tr valign="top">';
HUGE_TEST_STRING += '		<td width="238"><a href="/si/index.html?cnn=yes"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/sports.gif" alt="Sports News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/cnnsi/golfonline/2003/us_open/news/2003/06/12/open_thursday_ap">Woods eyes third U.S. Open title</a><br>';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/cnnsi/basketball/news/2003/06/12/jordan_ruling_ap">Judge denies Jordan&#039;s former lover $5M payoff</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td width="238"><a href="/money/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/business.gif" alt="Business News: " width="238" height="15" border="0"></a><br><div class="cnnMainSections">';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/money/2003/06/12/pf/saving/duppies/index.htm">Here come the "Duppies"</a><br>';
HUGE_TEST_STRING += '&#8226;&nbsp;<a href="/money/2003/06/12/technology/oracle/index.htm">Oracle beats estimates</a><br>';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '		<td><img src="http://i.cnn.net/cnn/images/1.gif" width="10" hspace="0" vspace="0" alt=""></td>';
HUGE_TEST_STRING += '		<td valign="top">';
HUGE_TEST_STRING += '		<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="267" height="2"></div>';
HUGE_TEST_STRING += '				';
HUGE_TEST_STRING += '<table width="267" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td width="173" bgcolor="#003366"><div class="cnnBlueBoxHeader"><span class="cnnBigPrint"><b>WATCH CNN TV</b></span></div></td>';
HUGE_TEST_STRING += '		<td width="25" class="cnnBlueBoxHeader" align="right"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/diagonal.gif" width="25" height="19" alt=""></td>';
HUGE_TEST_STRING += '		<td width="69" class="cnnBlueBoxTab" align="right" bgcolor="#336699"><a href="/CNN/Programs/"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/tv.schedule.gif" alt="On CNN TV" border="0" width="65" height="11" hspace="2" vspace="2" align="right"></a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '<table width="267" bgcolor="#EEEEEE" border="0" cellpadding="4" cellspacing="0">';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td><a href="/CNN/Programs/american.morning/"><img src="http://i.cnn.net/cnn/CNN/Programs/includes/showbox/images/2003/05/tz.hemmer.jpg" alt="American Morning, 7 a.m. ET" width="65" height="49" border="0" align="right"></a><a href="/CNN/Programs/american.morning/"><b>American Morning (7 a.m. ET):</b></a> Tomorrow, singer Carnie Wilson talks about her new book, "Im Still Hungry."';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += '[[table width="267" border="0" cellpadding="0" cellspacing="0"]]';
HUGE_TEST_STRING += '[[tr]][[td width="173" bgcolor="#003366"]][[div class="cnnBlueBoxHeader"]][[span class="cnnBigPrint"]][[b]]WATCH CNN TV[[/b]][[/span]][[/div]][[/td]][[td width="25" class="cnnBlueBoxHeader" align="right"]][[img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/diagonal.gif" width="25" height="19" alt=""]][[/td]][[td width="69" class="cnnBlueBoxTab" align="right" bgcolor="#336699"]][[a href="/CNN/Programs/"]][[img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/tv.schedule.gif" alt="On CNN TV" border="0" width="65" height="11" hspace="2" vspace="2" align="right"]][[/a]][[/td]][[/tr]][[/table]][[table width="267" bgcolor="#EEEEEE" border="0" cellpadding="4" cellspacing="0"]][[tr valign="top"]][[td]]';
HUGE_TEST_STRING += '[[img src="http://i.cnn.net/cnn/2003/images/05/31/tz.bw.jpg" alt="" width="65" height="49" border="0" align="right"]]';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '[[b]] CNN Presents: The Hunt for Eric Robert Rudolph (8 p.m. ET)[[/b]][[br]]Latest on his capture.';
HUGE_TEST_STRING += '					[[/td]]';
HUGE_TEST_STRING += '				[[/tr]]';
HUGE_TEST_STRING += '			[[/table]]';
HUGE_TEST_STRING += '-->';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '				<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>	';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '				<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="267" height="2"></div>';
HUGE_TEST_STRING += '				<table width="267" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '					<tr>';
HUGE_TEST_STRING += '						<td width="184" bgcolor="#003366"><div class="cnnBlueBoxHeader"><span class="cnnBigPrint"><b>ANALYSIS</b></span></div></td>';
HUGE_TEST_STRING += '						<td width="25" class="cnnBlueBoxHeader" align="right"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/diagonal.gif" width="25" height="19" alt=""></td>';
HUGE_TEST_STRING += '						<td width="58" class="cnnBlueBoxTab" align="right" bgcolor="#336699"><a href="/US"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/superlinks/us.gif" alt="U.S. News" border="0" width="54" height="11" hspace="2" vspace="2" align="right"></a></td>';
HUGE_TEST_STRING += '					</tr>';
HUGE_TEST_STRING += '				</table>';
HUGE_TEST_STRING += '				<table width="267" bgcolor="#EEEEEE" border="0" cellpadding="4" cellspacing="0">';
HUGE_TEST_STRING += '					<tr valign="top">';
HUGE_TEST_STRING += '						<td>';
HUGE_TEST_STRING += '<a href="/2003/US/06/12/nyt.safire/index.html"><img src="http://i.a.cnn.net/cnn/2003/US/06/12/nyt.safire/tz.stewart.jpg" alt="Fight It, Martha" width="65" height="49" border="0" align="right"></a>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<span class="cnnBodyText" style="font-weight:bold;color:#000;">NYTimes: </span> <a href="/2003/US/06/12/nyt.safire/index.html">Fight It, Martha</a><br>';
HUGE_TEST_STRING += 'William Safire: I hope Martha Stewart beats this bum rap';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '					</td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '			<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>';
HUGE_TEST_STRING += '				<div><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_c00.gif" alt="" width="267" height="2"></div>';
HUGE_TEST_STRING += '				<table width="267" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '					<tr>';
HUGE_TEST_STRING += '						<td width="164" bgcolor="#003366"><div class="cnnBlueBoxHeader"><span class="cnnBigPrint"><b>OFFBEAT</b></span></div></td>';
HUGE_TEST_STRING += '						<td width="25" class="cnnBlueBoxHeader" align="right"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/diagonal.gif" width="25" height="19" alt=""></td>';
HUGE_TEST_STRING += '						<td width="78" class="cnnBlueBoxTab" align="right" bgcolor="#336699"><a href="/offbeat"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/superlinks/offbeat.gif" alt="more offbeat" width="74" height="11" border="0" hspace="2" vspace="2" align="right"></a></td>';
HUGE_TEST_STRING += '					</tr>';
HUGE_TEST_STRING += '				</table>';
HUGE_TEST_STRING += '				<table width="267" bgcolor="#DDDDDD" border="0" cellpadding="4" cellspacing="0">';
HUGE_TEST_STRING += '					<tr valign="top">';
HUGE_TEST_STRING += '						<td>';
HUGE_TEST_STRING += '<a href="/2003/HEALTH/06/12/offbeat.china.sperm.ap/index.html"><img src="http://i.a.cnn.net/cnn/2003/HEALTH/06/12/offbeat.china.sperm.ap/tz.china.sperm.jpg" alt="Waiting list" width="65" height="49" border="0" align="right"></a>';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += ' <a href="/2003/HEALTH/06/12/offbeat.china.sperm.ap/index.html">Waiting list</a><br>';
HUGE_TEST_STRING += 'Chinas "smart sperm" bank needs donors';
HUGE_TEST_STRING += '					</td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '			<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="10"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '			<table width="267" bgcolor="#999999" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '				<tr>';
HUGE_TEST_STRING += '					<td>';
HUGE_TEST_STRING += '						<table width="100%" border="0" cellpadding="4" cellspacing="1">';
HUGE_TEST_STRING += '							<tr>';
HUGE_TEST_STRING += '								<td bgcolor="#EEEEEE" class="cnnMainWeatherBox"><a name="weatherBox"></a>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="257" border="0" cellpadding="1" cellspacing="0">';
HUGE_TEST_STRING += '<form method="get" action="http://weather.cnn.com/weather/search" style="margin: 0px">';
HUGE_TEST_STRING += '<input type="hidden" name="mode" value="hplwp">';
HUGE_TEST_STRING += '  <tr>';
HUGE_TEST_STRING += '    <td bgcolor="#FFFFFF"><table width="255" bgcolor="#EAEFF4" border="0" cellpadding="4" cellspacing="0">';
HUGE_TEST_STRING += '        <tr>';
HUGE_TEST_STRING += '          <td colspan="2" class="cnnWEATHERrow">&nbsp;<span class="cnnBigPrint">WEATHER</span></td>';
HUGE_TEST_STRING += '        </tr>';
HUGE_TEST_STRING += '        <tr>';
HUGE_TEST_STRING += '          <td colspan="2" class="cnnBodyText">Get your hometown weather on the home page! <b>Enter city name or U.S. Zip Code:</b></td>';
HUGE_TEST_STRING += '        </tr>';
HUGE_TEST_STRING += '        <tr>';
HUGE_TEST_STRING += '          <td><input class="cnnFormText" type="text" size="12" name="wsearch" value="" style="width:100px;"></td>';
HUGE_TEST_STRING += '          <td><input class="cnnNavButton" type="submit" value="PERSONALIZE"></td>';
HUGE_TEST_STRING += '        </tr>';
HUGE_TEST_STRING += '        <tr>';
HUGE_TEST_STRING += '          <td class="cnnBodyText" colspan="2">Or <a href="javascript:CNN_openPopup("http://weather.cnn.com/weather/select.popup/content2.jsp?mode=hplwp", "weather", "toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=260,height=250")"><b>select location from a list</b></a></td>';
HUGE_TEST_STRING += '        </tr>';
HUGE_TEST_STRING += '    </table></td>';
HUGE_TEST_STRING += '  </tr>';
HUGE_TEST_STRING += '</form>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '								</td>';
HUGE_TEST_STRING += '							</tr>';
HUGE_TEST_STRING += '							<tr>';
HUGE_TEST_STRING += '								<td bgcolor="#EEEEEE">';
HUGE_TEST_STRING += '									<table width="100%" border="0" cellpadding="0" cellspacing="2">';
HUGE_TEST_STRING += '										<tr>';
HUGE_TEST_STRING += '											<td><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/quickvote.gif" alt="Quick Vote" width="107" height="24" border="0"></td>';
HUGE_TEST_STRING += '											<td width="88" align="right"><!-- ad home/quickvote/sponsor.88x31 -->';
HUGE_TEST_STRING += '<!-- ad commented while aol investigates 3/31/03 5:40 a.m. lk -->';
HUGE_TEST_STRING += '<a href="http://ar.atwola.com/link/93101912/aol"><img src="http://ar.atwola.com/image/93101912/aol" alt="Click Here" width="88" height="31" border="0" hspace="0" vspace="0"></a>';
HUGE_TEST_STRING += '</td>';
HUGE_TEST_STRING += '										</tr>';
HUGE_TEST_STRING += '									</table>';
HUGE_TEST_STRING += '<table width="100%" cellspacing="0" cellpadding="1" border="0"><form target="popuppoll" method="post" action="http://polls.cnn.com/poll">';
HUGE_TEST_STRING += '<INPUT TYPE=HIDDEN NAME="poll_id" VALUE="3966">';
HUGE_TEST_STRING += '<tr><td colspan="2" align="left"><span class="cnnBodyText">Should an international peacekeeping force be sent to the Mideast?<br></span></td></tr>';
HUGE_TEST_STRING += '<tr valign="top">';
HUGE_TEST_STRING += '<td><span class="cnnBodyText">Yes</span>';
HUGE_TEST_STRING += '</td><td align="right"><input value="1" type="radio" name="question_1"></td></tr>';
HUGE_TEST_STRING += '<tr valign="top">';
HUGE_TEST_STRING += '<td><span class="cnnBodyText">No</span>';
HUGE_TEST_STRING += '</td><td align="right"><input value="2" type="radio" name="question_1"></td></tr>';
HUGE_TEST_STRING += '<!-- /end Question 1 -->';
HUGE_TEST_STRING += '<tr>';
HUGE_TEST_STRING += '<td colspan="2">';
HUGE_TEST_STRING += '<table width="100%" cellspacing="0" cellpadding="0" border="0"><tr><td><span class="cnnInterfaceLink"><nobr><a href="javascript:CNN_openPopup("/POLLSERVER/results/3966.html","popuppoll","toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=510,height=400")">VIEW RESULTS</a></nobr></span></td>';
HUGE_TEST_STRING += '<td align="right"><input class="cnnFormButton" onclick="CNN_openPopup("","popuppoll","toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=510,height=400")" value="VOTE" type="SUBMIT"></td></tr></table></td></tr>';
HUGE_TEST_STRING += '</form></table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '								</td>';
HUGE_TEST_STRING += '							</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '					</td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table>';
HUGE_TEST_STRING += '		<!-- /right --></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td colspan="3" valign="bottom">		<img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/px_ccc.gif" alt="" width="483" height="1">		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '<table width="770" border="0" cellpadding="0" cellspacing="0" summary="Links to stories from CNN partners">';
HUGE_TEST_STRING += '	<col width="10">';
HUGE_TEST_STRING += '	<col width="250" align="left" valign="top">';
HUGE_TEST_STRING += '	<col width="5">';
HUGE_TEST_STRING += '	<col width="250" align="left" valign="top">';
HUGE_TEST_STRING += '	<col width="5">';
HUGE_TEST_STRING += '	<col width="250" align="left" valign="top">';
HUGE_TEST_STRING += '	<tr><td colspan="6"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="2"></td></tr>';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td rowspan="6" width="10"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="10" height="1"></td>';
HUGE_TEST_STRING += '		<td colspan="3"><span class="cnnMenuText" style="font-size: 12px"><b style="color: #c00">From our Partners</b></span>';
HUGE_TEST_STRING += '			<img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/icon_external.gif" alt=" External site icon " width="20" height="13" border="0" align="middle"></td>';
HUGE_TEST_STRING += '		<td colspan="2"></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '	<tr><td colspan="5"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="2"></td></tr>';
HUGE_TEST_STRING += '	<tr><td colspan="5" bgcolor="#CCCCCC"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1"></td></tr>';
HUGE_TEST_STRING += '	<tr><td colspan="5"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="2"></td></tr>';
HUGE_TEST_STRING += '	<tr valign="top">';
HUGE_TEST_STRING += '		<td class="cnnMainSections" width="250">';
HUGE_TEST_STRING += '<a href="/time/" target="new"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/partner_time.gif" alt="Time: " width="70" height="17" border="0"></a><br><div style="margin-top: 4px">	&#8226;&nbsp;<a target="new" href="/time/magazine/article/0,9171,1101030616-457387,00.html?CNN=yes">Where the Jobs Are</a><br>	&#8226;&nbsp;<a target="new" href="/time/magazine/article/0,9171,1101030616-457373,00.html?CNN=yes">Of Dogs and Men</a><br>	&#8226;&nbsp;<a target="new" href="/time/photoessays/gunmen/?CNN=yes">Photo Essay: Fighting the Peace</a><br></div><table border="0"><tr><td><img height="1" width="1" alt="" src="http://i.cnn.net/cnn/images/1.gif"/></td></tr><tr bgcolor="#dddddd"><td>&nbsp;&nbsp;<a target="new" href="/linkto/time.main.html">Subscribe to TIME</a>&nbsp;&nbsp;</td></tr></table>		</td>';
HUGE_TEST_STRING += '		<td width="5"><br></td>';
HUGE_TEST_STRING += '		<td class="cnnMainSections" width="250">';
HUGE_TEST_STRING += '<a href="/cnnsi/index.html?cnn=yes"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/partner_si.gif" alt="CNNsi.com: " width="138" height="17" border="0"></a><br><div style="margin-top: 4px">';
HUGE_TEST_STRING += '&#8226;&nbsp;Marty Burns: <a target="new" href="/cnnsi/inside_game/marty_burns/news/2003/06/11/burns_game4/">Nets pull out all stops</a><br>';
HUGE_TEST_STRING += '&#8226;&nbsp;Michael Farber: <a target="new" href="/cnnsi/inside_game/michael_farber/news/2003/06/11/farber_wrapup/">Sens look good for "04</a><br>';
HUGE_TEST_STRING += '&#8226;&nbsp;Tim Layden: <a target="new" href="/cnnsi/inside_game/tim_layden/news/2003/06/11/layden_neuheisel/">NFL or bust for Neuheisel</a><br>';
HUGE_TEST_STRING += '</div>';
HUGE_TEST_STRING += '<table border="0"><tr><td><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1"></td></tr><tr bgcolor="#dddddd"><td>&nbsp;&nbsp;<a href="http://subs.timeinc.net/CampaignHandler/si_cnnsi?source_id=19">Subscribe to Sports Illustrated</a>&nbsp;&nbsp;</td></tr></table>';
HUGE_TEST_STRING += '		</td>';
HUGE_TEST_STRING += '		<td width="5"><br></td>';
HUGE_TEST_STRING += '		<td class="cnnMainSections" width="250">';
HUGE_TEST_STRING += '<a href="/linkto/nyt/main.banner.html" target="new"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/partners_nyt.gif" alt="New York Times: " width="105" height="17" border="0"></a><br><div style="margin-top: 4px">	&#8226;&nbsp;<a target="new" href="/linkto/nyt/story/1.0612.html">U.S. Widens Checks at Foreign Ports</a><br>	&#8226;&nbsp;<a target="new" href="/linkto/nyt/story/2.0612.html">Rumsfeld: Iran Developing Nuclear Arms</a><br>	&#8226;&nbsp;<a target="new" href="/linkto/nyt/story/3.0612.html">Vandalism, "Improvements" Mar Great Wall</a><br></div><table border="0"><tr><td><img height="1" width="1" alt="" src="http://i.cnn.net/cnn/images/1.gif"/></td></tr><tr bgcolor="#dddddd"><td>&nbsp;&nbsp;<a target="new" href="/linkto/nyt.main.html">Get 50% OFF the NY Times</a>&nbsp;&nbsp;</td></tr></table>		</td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="2"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="770" border="0" cellpadding="0" cellspacing="0">';
HUGE_TEST_STRING += '	<tr>';
HUGE_TEST_STRING += '		<td width="10"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="10" height="10"></td>';
HUGE_TEST_STRING += '		<td width="760">';
HUGE_TEST_STRING += '<!-- floor -->';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="100%" border="0" cellpadding="0" cellspacing="0"><tr><td bgcolor="#999999"><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1"></td></tr></table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="100%" bgcolor="#DEDEDE" border="0" cellpadding="3" cellspacing="0">';
HUGE_TEST_STRING += '	<tr> ';
HUGE_TEST_STRING += '		<td><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="5" height="5"></td>';
HUGE_TEST_STRING += '		<td><a href="http://edition.cnn.com/" class="cnnFormTextB" onClick="clickEdLink()" style="color:#000;">International Edition</a></td>';
HUGE_TEST_STRING += '<form>';
HUGE_TEST_STRING += '		<td><select title="CNN.com is available in different languages" class="cnnMenuText" name="languages" size="1" style="font-weight: bold; vertical-align: middle" onChange="if (this.options[selectedIndex].value != "") location.href=this.options[selectedIndex].value">';
HUGE_TEST_STRING += '				<option value="" disabled selected>Languages</option>';
HUGE_TEST_STRING += '				<option value="" disabled>---------</option>';
HUGE_TEST_STRING += '				<option value="/cnnes/">Spanish</option>';
HUGE_TEST_STRING += '				<option value="http://cnn.de/">German</option>';
HUGE_TEST_STRING += '				<option value="http://cnnitalia.it/">Italian</option>';
HUGE_TEST_STRING += '				<option value="http://www.joins.com/cnn/">Korean</option>';
HUGE_TEST_STRING += '				<option value="http://arabic.cnn.com/">Arabic</option>';
HUGE_TEST_STRING += '				<option value="http://www.CNN.co.jp/">Japanese</option>';
HUGE_TEST_STRING += '			</select></td>';
HUGE_TEST_STRING += '</form>';
HUGE_TEST_STRING += '		<td><a href="/CNN/Programs/" class="cnnFormTextB" style="color:#000;">CNN TV</a></td>';
HUGE_TEST_STRING += '		<td><a href="/CNNI/" class="cnnFormTextB" style="color:#000;">CNN International</a></td>';
HUGE_TEST_STRING += '		<td><a href="/HLN/" class="cnnFormTextB" style="color:#000;">Headline News</a></td>';
HUGE_TEST_STRING += '		<td><a href="/TRANSCRIPTS/" class="cnnFormTextB" style="color:#000;">Transcripts</a></td>';
HUGE_TEST_STRING += '		<td><a href="/services/preferences/" title="Customize your CNN.com experience" class="cnnFormTextB" style="color:#000;">Preferences</a></td>';
HUGE_TEST_STRING += '		<td><a href="/INDEX/about.us/" class="cnnFormTextB" style="color:#000;">About CNN.com</a></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<div><img src="http://i.cnn.net/cnn/images/1.gif" alt="" width="1" height="1"></div>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<table width="100%" bgcolor="#EFEFEF" border="0" cellpadding="4" cellspacing="0">';
HUGE_TEST_STRING += '	<tr valign="top"> ';
HUGE_TEST_STRING += '		<td style="padding-left:10px"><div class="cnnSectCopyright">';
HUGE_TEST_STRING += '<b>&copy; 2003 Cable News Network LP, LLLP.</b><br>';
HUGE_TEST_STRING += 'An AOL Time Warner Company. All Rights Reserved.<br>';
HUGE_TEST_STRING += '<a href="/interactive_legal.html">Terms</a> under which this service is provided to you.<br>';
HUGE_TEST_STRING += 'Read our <a href="/privacy.html">privacy guidelines</a>. <a href="/feedback/">Contact us</a>.';
HUGE_TEST_STRING += '		</div></td>';
HUGE_TEST_STRING += '		<td align="right"><table border="0" cellpadding="4" cellspacing="0">';
HUGE_TEST_STRING += '				<tr> ';
HUGE_TEST_STRING += '					<td rowspan="2" align="middle"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/sect/SEARCH/dotted.line.gif" alt="" width="7" height="46"></td>';
HUGE_TEST_STRING += '					<td><img src="http://i.a.cnn.net/cnn/.element/img/1.0/misc/icon.external.links.gif" alt="external link" width="20" height="13"></td>';
HUGE_TEST_STRING += '					<td><div class="cnnSectExtSites">All external sites will open in a new browser.<br>';
HUGE_TEST_STRING += '							CNN.com does not endorse external sites.</div></td>';
HUGE_TEST_STRING += '					<td rowspan="2" align="middle"><img src="http://i.a.cnn.net/cnn/.element/img/1.0/sect/SEARCH/dotted.line.gif" alt="" width="7" height="46"></td>';
HUGE_TEST_STRING += '					<td rowspan="2"><!-- home/powered_by/sponsor.88x31 -->';
HUGE_TEST_STRING += '<script language="JavaScript1.1">';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += 'adSetTarget("_top");';
HUGE_TEST_STRING += 'htmlAdWH( (new Array(93103308,93103308,93103308,93103308))[document.adoffset||0] , 88, 31);';
HUGE_TEST_STRING += '//-->';
HUGE_TEST_STRING += '</script><noscript><a href="http://ar.atwola.com/link/93103308/aol" target="_top"><img src="http://ar.atwola.com/image/93103308/aol" alt="Click here for our advertiser" width="88" height="31" border="0"></a></noscript>';
HUGE_TEST_STRING += '</td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '				<tr valign="top"> ';
HUGE_TEST_STRING += '					<td><img src="http://i.a.cnn.net/cnn/.element/img/1.0/main/icon_premium.gif" alt=" Premium content icon " width="9" height="11"></td>';
HUGE_TEST_STRING += '					<td><span class="cnnSectExtSites">Denotes premium content.</span></td>';
HUGE_TEST_STRING += '				</tr>';
HUGE_TEST_STRING += '			</table></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- /floor --></td>';
HUGE_TEST_STRING += '	</tr>';
HUGE_TEST_STRING += '</table>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<!-- popunder ad generic/popunder_launch.720x300 -->';
HUGE_TEST_STRING += '<script language="JavaScript1.1" type="text/javascript">';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += 'if (document.adPopupFile) {';
HUGE_TEST_STRING += '	if (document.adPopupInterval == null) {';
HUGE_TEST_STRING += '		document.adPopupInterval = "0";';
HUGE_TEST_STRING += '	}';
HUGE_TEST_STRING += '	if (document.adPopunderInterval == null) {';
HUGE_TEST_STRING += '		document.adPopunderInterval = document.adPopupInterval;';
HUGE_TEST_STRING += '	}';
HUGE_TEST_STRING += '	if (document.adPopupDomain != null) {';
HUGE_TEST_STRING += '		adSetPopDm(document.adPopupDomain);';
HUGE_TEST_STRING += '	}';
HUGE_TEST_STRING += '	adSetPopupWH("93162673", "720", "300", document.adPopupFile, document.adPopunderInterval, 20, 50, -1);';
HUGE_TEST_STRING += '}';
HUGE_TEST_STRING += '// -->';
HUGE_TEST_STRING += '</script>';
HUGE_TEST_STRING += '	';
HUGE_TEST_STRING += '<!-- home/bottom.eyeblaster -->';
HUGE_TEST_STRING += '<script language="JavaScript1.1" type="text/javascript">';
HUGE_TEST_STRING += '<!--';
HUGE_TEST_STRING += 'var MacPPC = (navigator.platform == "MacPPC") ? true : false;';
HUGE_TEST_STRING += 'if (!MacPPC) {';
HUGE_TEST_STRING += 'adSetType("J");';
HUGE_TEST_STRING += 'htmlAdWH( (new Array(93137910,93137910,93137910,93137910))[document.adoffset||0], 101, 1);';
HUGE_TEST_STRING += 'adSetType("");';
HUGE_TEST_STRING += '}';
HUGE_TEST_STRING += '// -->';
HUGE_TEST_STRING += '</script>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<script language="JavaScript1.1" src="http://ar.atwola.com/file/adsEnd.js"></script>';
HUGE_TEST_STRING += '';
HUGE_TEST_STRING += '<img src="/cookie.crumb" alt="" width="1" height="1">';
HUGE_TEST_STRING += '<!--include virtual="/virtual/2002/main/survey.html"-->';
HUGE_TEST_STRING += '</body>';
HUGE_TEST_STRING += '</html>';

return HUGE_TEST_STRING;
}