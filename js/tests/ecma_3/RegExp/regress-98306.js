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
* Contributor(s): jrgm@netscape.com, pschwartau@netscape.com
* Date: 04 September 2001
*
* SUMMARY: Regression test for Bugzilla bug 98306:
* "JS parser crashes in ParseAtom for script using Regexp()"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=98306
* As noted there, we could generate this crash with just one line:
*
*                 "Hello".match(/[/]/);
*
* However, we include the longer testcase as originally reported -
*/
//-----------------------------------------------------------------------------
var bug = 98306;
var summary = "Testing that we don't crash on this code -";


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);

  // This caused an immediate crash -
  var obj1 = new BrowserData('Hello');

  // Here is a more realistic string. Test this, too -
  var sUserAgent = 'Mozilla/5.0 (Windows; U; WinNT4.0; en-US; rv:0.9.3+) Gecko/20010828';
  var obj2 = new BrowserData(sUserAgent);

  // This alone also caused a crash - 
  "Hello".match(/[/]/);

  exitFunc ('test');
}


/*
 *  BrowserData() constructor
 *
 * .userAgent: (string) the HTTP_USER_AGENT input string
 * .browser: (string) "MSIE", "Opera", "Nav", "Other"
 * .majorVer: (integer) major version
 * .minorVer: (string) minor (dot) version
 * .betaVer: (string) beta version
 * .platform: (string) operating system
 * .getsNavBar (boolean) whether browser gets the DHTML menus
 * .doesActiveX (boolean) whether browser does 32-bit ActiveX
 */
function BrowserData(sUA)
{
  this.userAgent = sUA.toString();
  var rPattern = /(MSIE)\s(\d+)\.(\d+)((b|p)([^(s|;)]+))?;?(.*(98|95|NT|3.1|32|Mac|X11))?\s*([^\)]*)/;

  if (this.userAgent.match(rPattern))
  {
    this.browser = "MSIE";
    this.majorVer = parseInt(RegExp.$2) || 0;
    this.minorVer = RegExp.$3.toString() || "0";
    this.betaVer = RegExp.$6.toString() || "0";
    this.platform = RegExp.$8 || "Other";
    this.platVer = RegExp.$9 || "0";
  }
  else if (this.userAgent.match(/Mozilla[/].*(95[/]NT|95|NT|98|3.1).*Opera.*(\d+)\.(\d+)/))
  {
    //"Mozilla/4.0 (Windows NT 5.0;US) Opera 3.60  [en]";
    this.browser = "Opera";
    this.majorVer = parseInt(RegExp.$2) || parseInt(RegExp.$2) || 0;
    this.minorVer = RegExp.$3.toString() || RegExp.$3.toString() || "0";
    this.platform = RegExp.$1 || "Other";
  }
  else if (this.userAgent.match(/Mozilla[/](\d*)\.?(\d*)(.*(98|95|NT|32|16|68K|PPC|X11))?/))
  {
    //"Mozilla/4.5 [en] (WinNT; I)"
    this.browser = "Nav";
    this.majorVer = parseInt(RegExp.$1) || 0;
    this.minorVer = RegExp.$2.toString() || "0";
    this.platform = RegExp.$4 || "Other";
  }
  else
  {
    this.browser = "Other";
  }

  this.getsNavBar = ("MSIE" == this.browser && 4 <= this.majorVer &&
                     "Mac" != this.platform && "X11" != this.platform);
  
  this.doesActiveX = ("MSIE" == this.browser && 3 <= this.majorVer &&
                     ("95" == this.platform || "98" == this.platform || "NT" == this.platform));
  
  this.fullVer = parseFloat( this.majorVer + "." + this.minorVer );

  this.doesPersistence = ("MSIE" == this.browser && 5 <= this.majorVer &&
                          "Mac" != this.platform && "X11" != this.platform);
}
