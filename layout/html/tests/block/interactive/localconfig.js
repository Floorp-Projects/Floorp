//
// This file contains the installation specific values for QuickSearch.
// See quicksearch.js for more details.
//

// the global bugzilla url

//var bugzilla = "";
var bugzilla = "http://bugzilla.mozilla.org/";

// Status and Resolution
// =====================

var statuses_open     = new Array("UNCONFIRMED","NEW","ASSIGNED","REOPENED");
var statuses_resolved = new Array("RESOLVED","VERIFIED","CLOSED");
var resolutions       = new Array("FIXED","INVALID","WONTFIX","LATER",
                                  "REMIND","DUPLICATE","WORKSFORME","MOVED");

// Keywords
// ========
//
// Enumerate all your keywords here. This is necessary to avoid 
// "foo is not a legal keyword" errors. This makes it possible
// to include the keywords field in the search by default.

var keywords = new Array(
"4xp"
,"64bit"
,"access"
,"approval"
,"arch"
,"classic"
,"compat"
,"crash"
,"css-moz"
,"css1"
,"css2"
,"css3"
,"dataloss"
,"dom0"
,"dom1"
,"dom2"
,"ecommerce"
,"embed"
,"fcc508"
,"fonts"
,"footprint"
,"hang"
,"helpwanted"
,"highrisk"
,"html4"
,"interop"
,"intl"
,"js1.5"
,"l12y"
,"mail1"
,"mail2"
,"mail3"
,"mail4"
,"mail5"
,"mail6"
,"mailtrack"
,"meta"
,"mlk"
,"modern"
,"mozilla0.9.3"
,"mozilla0.9.4"
,"mozilla0.9.5"
,"mozilla0.9.6"
,"mozilla0.9.7"
,"mozilla1.0"
,"mozilla1.0.1"
,"mozilla1.1"
,"mozilla1.2"
,"nsbeta1"
,"nsbeta1+"
,"nsbeta1-"
,"nsbeta2"
,"nsbeta3"
,"nsbranch"
,"nsbranch+"
,"nsbranch-"
,"nsCatFood"
,"nsCatFood+"
,"nsCatFood-"
,"nsdogfood"
,"nsdogfood+"
,"nsdogfood-"
,"nsenterprise"
,"nsenterprise+"
,"nsenterprise-"
,"nsmac1"
,"nsmac2"
,"nsonly"
,"nsrtm"
,"oeone"
,"patch"
,"perf"
,"polish"
,"pp"
,"privacy"
,"qawanted"
,"realplayer"
,"regression"
,"relnote"
,"relnote2"
,"relnote3"
,"relnoteRTM"
,"review"
,"shockwave"
,"smoketest"
,"stackwanted"
,"testcase"
,"top100"
,"topcrash"
,"topembed"
,"topmlk"
,"topperf"
,"ui"
,"useless-UI"
,"vbranch"
,"verifyme"
,"vtrunk"
,"xhtml"
);

// Platforms
// =========
//
// A list of words <w> (substrings of platform values) 
// that will automatically be translated to "platform:<w>" 
// E.g. if "mac" is defined as a platform, then searching 
// for it will find all bugs with platform="Macintosh", 
// but no other bugs with e.g. "mac" in the summary.

var platforms = new Array(//"all", //this is a legal value for OpSys, too :(
                          "dec","hp","mac", //shortcut added
                          "macintosh","pc","sgi","sun");

// Severities
// ==========
//
// A list of words <w> (substrings of severity values)
// that will automatically be translated to "severity:<w>"
// E.g with this default set of severities, searching for
// "blo,cri,maj" will find all severe bugs.

var severities = new Array("blo","cri","maj","nor","min","tri","enh");

// Products and Components
// =======================
//
// It is not necessary to list all products and components here.
// Instead, you can define a "blacklist" for some commonly used 
// words or word fragments that occur in a product or component name
// but should _not_ trigger product/component search.

var product_exceptions = new Array(
"row"   // [Browser]
        //   ^^^
,"new"  // [MailNews]
        //      ^^^
);

var component_exceptions = new Array(
"hang"  // [mozilla.org] Bugzilla: Component/Keyword Changes
        //                                            ^^^^
);

