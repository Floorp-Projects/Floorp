// XMLTerm Page Commands

// CONVENTION: All pre-defined XMLTerm Javascript functions and global
//             variables should begin with an upper-case letter.
//             This would allow them to be easily distinguished from
//             user defined functions, which should begin with a lower case
//             letter.

// Global variables
///var gStyleRuleNames;

WRITE_LOG = function (str) {};
DEBUG_LOG = function (str) {};

if (navigator) {
   var userAgent = navigator.userAgent;

   if (userAgent && (userAgent.search(/Mozilla\/4\./i) == -1)) {

     if (dump) {
        WRITE_LOG = dump;
     }
  }
} else {
  if (dump) {
     WRITE_LOG = dump;
  }
}

var Tips = new Array();       // Usage tip strings
var TipNames = new Array();   // Usage tip names
var TipCount = 0;             // No. of tips
var SelectedTip = 0;          // Selected random tip

// Set prompt using form entry
function DefineTip(tip, name) {
  Tips[TipCount] = tip;
  TipNames[TipCount] = name;
  TipCount++;
  return;
}

DefineTip('Click the new tip link to the left to get a new tip!',
          'tips');

DefineTip('User level setting (at the top) controls amount of help information',
         'level');

DefineTip('Beginner level setting displays keyboard shortcuts at the top of the page',
         'level');

DefineTip('Icons setting controls whether directory listings use icons',
          'icons');

DefineTip('Windows setting controls whether double-clicking creates new windows',
          'windows');

DefineTip('Single click an explicit (underlined) hyperlink; double click implicit (usually blue) hyperlinks',
         'clicking');

DefineTip('Clicking on command prompt expands/collapses command output display.',
          'prompt');

DefineTip('Press F1 (or control-Home) key to collapse output of all commands.',
          'prompt');

DefineTip('"js:SetPrompt(HTMLstring);" sets prompt to any HTML string.',
          'prompt');

DefineTip('Beginners may click the SetPrompt button for a cool Mozilla prompt from dmoz.org!',
         'prompt');

DefineTip('Double-clicking on a previous command line re-executes the command.',
          'command');

DefineTip('Type "js:func(arg);" to execute inline Javascript function func.',
          'js');

DefineTip('Inline Javascript ("js:...") can be used to produce HTML output.',
          'js');

DefineTip('XMLterm supports full screen commands like "less", "vi", and "emacs -nw"".',
         'full-screen');

DefineTip('"xls" produces a clickable listing of directory contents.',
          'xls');

DefineTip('"xls -t" prevents display of icons even if Icons setting is "on".',
          'xls');

DefineTip('"xcat text-file" displays a text file with clickable URLs.',
          'xcat');

DefineTip('"xcat image.gif" displays an image file inline!',
          'xcat');

DefineTip('"xcat http://mozilla.org" displays a web page inline using IFRAMEs.',
          'xcat');

DefineTip('"xcat -h 1000 http://mozilla.org" displays using an IFRAME 1000 pixels high.',
          'xcat');

DefineTip('"xcat doc.html" display an HTML document inline using IFRAMEs.',
          'xcat');

// Display random usage tip
// (should cease to operate after a few cycles;
// need to use Prefs to keep track of that)
function NewTip() {
  var ranval = Math.random();
  SelectedTip = Math.floor(ranval * TipCount);
  if (SelectedTip >= TipCount) SelectedTip = 0;

  DEBUG_LOG("xmlterm: NewTip "+SelectedTip+","+ranval+"\n");

  var tipdata = document.getElementById('tipdata');
  tipdata.firstChild.data = Tips[SelectedTip];

  ShowHelp("",0);

  return false;
}

// Explain tip
function ExplainTip(tipName) {
  DEBUG_LOG("xmlterm: ExplainTip("+tipName+")\n");

  if (tipName) {
    var tipdata = document.getElementById('tipdata');
    tipdata.firstChild.data = "";
  } else {
    tipName = TipNames[SelectedTip];
  }

  ShowHelp('xmltermTips.html#'+tipName,0,120);

  return false;
}

// F1 key - Hide all output
function F1Key(isShift, isControl) {
  return DisplayAllOutput(false);
}

// F2 key - Show all output
function F2Key(isShift, isControl) {
  return DisplayAllOutput(true);
}

// F7 - Explain tip
function F7Key(isShift, isControl) {
  return ExplainTip();
}

// F8 - New tip
function F8Key(isShift, isControl) {
  return NewTip();
}

// F9 key
function F9Key(isShift, isControl) {
  return NewXMLTerm('');
}

// Scroll Home key
function ScrollHomeKey(isShift, isControl) {
  DEBUG_LOG("ScrollHomeKey("+isShift+","+isControl+")\n");

  if (isControl) {
    return F1Key(isShift, 0);
  } else {
    ScrollWin(isShift,isControl).scroll(0,0);
    return false;
  }
}

// Scroll End key
function ScrollEndKey(isShift, isControl) {
  DEBUG_LOG("ScrollEndKey("+isShift+","+isControl+")\n");

  if (isControl) {
    return F2Key(isShift, 0);
  } else {
    ScrollWin(isShift,isControl).scroll(0,99999);
    return false;
  }
}

// Scroll PageUp key
function ScrollPageUpKey(isShift, isControl) {
  DEBUG_LOG("ScrollPageUpKey("+isShift+","+isControl+")\n");

  ScrollWin(isShift,isControl).scrollBy(0,-120);
  return false;
}

// Scroll PageDown key
function ScrollPageDownKey(isShift, isControl) {
  DEBUG_LOG("ScrollPageDownKey("+isShift+","+isControl+")\n");

  ScrollWin(isShift,isControl).scrollBy(0,120);
  return false;
}

// Scroll Window
function ScrollWin(isShift, isControl) {
  if (isShift && (window.frames.length > 0)) {
    return window.frames[window.frames.length-1];
  } else {
    return window;
  }
}

// Set history buffer size
function SetHistory(value) {
  DEBUG_LOG("SetHistory("+value+")\n");
  window.xmlterm.setHistory(value, document.cookie);
  return false;
}

// Set prompt
function SetPrompt(value) {
  DEBUG_LOG("SetPrompt("+value+")\n");
  window.xmlterm.setPrompt(value, document.cookie);
  return false;
}

// Create new XMLTerm window
function NewXMLTerm(firstcommand) {
  newwin = window.openDialog( "chrome://xmlterm/content/xmlterm.xul",
                              "xmlterm", "chrome,dialog=no,resizable",
                              firstcommand);
  WRITE_LOG("NewXMLTerm: "+newwin+"\n")
  return false;
}

// Handle resize events
function Resize(event) {
  DEBUG_LOG("Resize()\n");
  window.xmlterm.resize();
  return false;
}

// Form Focus (workaround for form input being transmitted to xmlterm)
function FormFocus() {
  DEBUG_LOG("FormFocus()\n");
  window.xmlterm.ignoreKeyPress(true, document.cookie);
  return false;
}

// Form Blur (workaround for form input being transmitted to xmlterm)
function FormBlur() {
  DEBUG_LOG("FormBlur()\n");
  window.xmlterm.ignoreKeyPress(false, document.cookie);
  return false;
}

// Set user level, icons mode etc.
function UpdateSettings(selectName) {

   DEBUG_LOG("UpdateSettings: "+selectName+"\n");

   var selectedIndex = document.xmltform1[selectName].selectedIndex;
   var selectedOption = document.xmltform1[selectName].options[selectedIndex].value;

   DEBUG_LOG("UpdateSettings: selectedOption=="+selectedOption+"\n");

   switch(selectName) {
      case "userLevel":
         // Change user level

         if (selectedOption == "advanced") {
           AlterStyle("DIV.beginner",     "display", "none");
           AlterStyle("DIV.intermediate", "display", "none");
      
         } else if (selectedOption == "intermediate") {
           AlterStyle("DIV.intermediate", "display", "block");
           AlterStyle("DIV.beginner",     "display", "none");
      
         } else {
           AlterStyle("DIV.beginner",     "display", "block");
           AlterStyle("DIV.intermediate", "display", "block");
         }
      break;

      case "showIcons":
         // Change icon display style in the style sheet

         if (selectedOption == "on") {
            AlterStyle("SPAN.noicons", "display", "none");
            AlterStyle("SPAN.icons",   "display", "inline");
            AlterStyle("IMG.icons",    "display", "inline");
            AlterStyle("TR.icons",     "display", "table-row");

         } else {
            AlterStyle("SPAN.noicons", "display", "inline");
            AlterStyle("SPAN.icons",   "display", "none");
            AlterStyle("IMG.icons",    "display", "none");
            AlterStyle("TR.icons",     "display", "none");
         }
      break;

      case "windowsMode":
         break;

      default:
         DEBUG_LOG("UpdateSettings: Unknown selectName "+selectName+"\n");
         break;
   }

   window.xmltform1Index[selectName] = selectedIndex;
   window.xmltform1Option[selectName] = selectedOption;

   Webcast();

   return false;
}

// Alter style in stylesheet of specified document doc, or current document
// if doc is omitted.
function AlterStyle(ruleName, propertyName, propertyValue, doc) {

  DEBUG_LOG("AlterStyle("+ruleName+"{"+propertyName+":"+propertyValue+"})\n");

  if (!doc) doc = window.document;

  var sheet = doc.styleSheets[0];
  var r;
  for (r = 0; r < sheet.cssRules.length; r++) {
    DEBUG_LOG("selectorText["+r+"]="+sheet.cssRules[r].selectorText+"\n");

    if (sheet.cssRules[r].selectorText.toLowerCase() == ruleName.toLowerCase()) {

    // Ugly workaround for accessing rules until bug 53448 is fixed
    // (****CSS DOM IS BROKEN****; selectorText seems to be null for all rules)
    ///if (gStyleRuleNames[r] == ruleName) {

      DEBUG_LOG("cssText["+r+"]="+sheet.cssRules[r].cssText+"\n");
      var style = sheet.cssRules[r].style;
      DEBUG_LOG("style="+style.getPropertyValue(propertyName)+"\n");

      style.setProperty(propertyName,propertyValue,"");
    }
  }
  return false;
}

// Handle default meta command
function MetaDefault(arg1) {
  return Load("http://"+arg1);
}

// Load URL in XMLTermBrowser window
function Load(url) {
  DEBUG_LOG("Load:url="+url+"\n");
  window.open(url);

  return false;
}

// Control display of all output elements
function DisplayAllOutput(flag) {
  var outputElements = document.getElementsByName("output");
  for (i=0; i<outputElements.length-1; i++) {
    var outputElement = outputElements[i];
    ///outputElement.style.display = (flag) ? "block" : "none";

    if (flag) 
       outputElement.removeAttribute('xmlt-block-collapsed');
    else
       outputElement.setAttribute('xmlt-block-collapsed', 'true');
  }

  var promptElements = document.getElementsByName("prompt");
  for (i=0; i<promptElements.length-1; i++) {
    promptElement = promptElements[i];
    ///promptElement.style.setProperty("text-decoration",
    ///                                 (flag) ? "none" : "underline", "");

    if (flag) 
       promptElement.removeAttribute('xmlt-underline');
    else
       promptElement.setAttribute('xmlt-underline', 'true');
  }

  if (!flag) {
    ScrollHomeKey(0,0);
//    ScrollEndKey(0,0);
  }

  Webcast();

  return false;
}

// Centralized event handler
// eventType values:
//   click
//
// targetType: 
//   textlink  - hyperlink
//   prompt    - command prompt
//   command   - command line
//
//   (Following are inline or in new window depending upon window.xmltform1Option.windowsMode)
//   cdxls     - change directory and list contents   (doubleclick)
//   xcat      - display file                         (doubleclick)
//   exec      - execute file                         (doubleclick)
//
//   send      - transmit arg to LineTerm
//   sendln    - transmit arg+newline to LineTerm
//   createln  - transmit arg+newline as first command to new XMLTerm
//
// entryNumber: >=0 means process only if current entry
//              <0  means process anytime
//
// arg1:    relative pathname of file/directory
// arg2:    absolute pathname prefix (for use outside current entry;
//                      use relative pathname in current working directory)
//
function HandleEvent(eventObj, eventType, targetType, entryNumber,
                     arg1, arg2) {
  DEBUG_LOG("HandleEvent("+eventObj+","+eventType+","+targetType+","+entryNumber+","+arg1+","+arg2+")\n");

  // Entry independent targets
  if (targetType === "textlink") {
    // Single click opens hyperlink
    // Browser-style
    DEBUG_LOG("textlink = "+arg1+"\n");
    Load(arg1);

  } else if (targetType === "prompt") {
    // Single click on prompt expands/collapses command output
    var outputElement = document.getElementById("output"+entryNumber);
    var helpElement = document.getElementById("help"+entryNumber);
    var promptElement = document.getElementById("prompt"+entryNumber);
    DEBUG_LOG(promptElement.style.getPropertyValue("text-decoration"));

    var collapsed = outputElement.getAttribute('xmlt-block-collapsed');

    if (collapsed) {
    ///if (outputElement.style.display == "none") {
      ///outputElement.style.display = "block";
      ///promptElement.style.setProperty("text-decoration","none","");

      outputElement.removeAttribute('xmlt-block-collapsed');
      promptElement.removeAttribute('xmlt-underline');

    } else {
      ///outputElement.style.display = "none";
      ///promptElement.style.setProperty("text-decoration","underline","");

      outputElement.setAttribute('xmlt-block-collapsed', 'true');
      promptElement.setAttribute('xmlt-underline', 'true');
      if (helpElement) {
        ShowHelp("",entryNumber);
      }
    }

  } else if (eventType === "click") {

     DEBUG_LOG("clickCount="+eventObj.detail+"\n");

     var shiftClick = eventObj.shiftKey;
     var dblClick = (eventObj.detail == 2);

     // Execute shell commands only on double-click for safety
     // Use single click for "selection" and prompt expansion only
     // Windows-style

     var currentEntryNumber = window.xmlterm.currentEntryNumber;

     var currentCmdElement = document.getElementById("command"+currentEntryNumber);
     var currentCommandEmpty = true;
     if (currentCmdElement && currentCmdElement.hasChildNodes()) {

       if (currentCmdElement.firstChild.nodeType == Node.TEXT_NODE) {
         DEBUG_LOG("textLength = "+currentCmdElement.firstChild.data.length+"\n");
         currentCommandEmpty = (currentCmdElement.firstChild.data.length == 0);

       } else {
         currentCommandEmpty = false;
       }
     }
     DEBUG_LOG("empty = "+currentCommandEmpty+"\n");

     if (targetType === "command") {
       if (!dblClick)
         return false;
       var commandElement = document.getElementById(targetType+entryNumber);
       var command = commandElement.firstChild.data;
       if (currentCommandEmpty) {
         window.xmlterm.sendText("\025"+command+"\n", document.cookie);
       } else {
         window.xmlterm.sendText(command, document.cookie);
       }

     } else {
       // Targets which may be qualified only for current entry

       if ((entryNumber >= 0) &&
         (window.xmlterm.currentEntryNumber != entryNumber)) {
         DEBUG_LOG("NOT CURRENT COMMAND\n");
         return false;
       }

       var action, sendStr;

       if ( (targetType === "cdxls") ||
            (targetType === "xcat")  ||
            (targetType === "exec")    ) {
         // Complex commands

         if (!dblClick)
           return false;

         var filename;
         var isCurrentCommand = (Math.abs(entryNumber)+1 ==
                                 window.xmlterm.currentEntryNumber);

         if ( (arg2 != null) && 
              (!isCurrentCommand || (window.xmltform1Option.windowsMode === "on")) ) {
           // Full pathname
           filename = arg2+arg1;
         } else {
           // Short pathname
           filename = arg1;
           if (targetType === "exec")
             filename = "./"+filename;
         }

         var prefix, suffix;
         if (targetType === "cdxls") {
           // Change directory and list contents
           prefix = "cd ";
           suffix = "; xls";
         } else if (targetType === "xcat") {
           // Display file
           prefix = "xcat ";
           suffix = "";
         } else if (targetType === "exec") {
           // Execute file
           prefix = "";
           suffix = "";
         }

         if (shiftClick || (window.xmltform1Option.windowsMode === "on")) {
           action = "createln";
           sendStr = prefix + filename + suffix;

         } else if (currentCommandEmpty) {
           action = "sendln";
           sendStr = prefix + filename + suffix;

         } else {
           action = "send";
           sendStr = filename + " ";
         }

       } else {
         // Primitive action
         action = targetType;
         sendStr = arg1;
       }

       // Primitive actions
       if (action === "send") {
         DEBUG_LOG("send = "+sendStr+"\n");
         window.xmlterm.sendText(sendStr, document.cookie);

       } else if (action === "sendln") {
         DEBUG_LOG("sendln = "+sendStr+"\n\n");
         window.xmlterm.sendText("\025"+sendStr+"\n", document.cookie);

       } else if (action === "createln") {
         DEBUG_LOG("createln = "+sendStr+"\n\n");
         newwin = NewXMLTerm(sendStr+"\n");
       }
    }
  }

  Webcast();

  return false;
}

// Set history buffer count using form entry
function SetHistoryValue() {
  var field = document.getElementById('inputvalue');
  return SetHistory(field.value);
}

// Set prompt using form entry
function SetPromptValue() {
  var field = document.getElementById('inputvalue');
  return SetPrompt(field.value);
}

function Webcast() {
  if (!window.webcastIntervalId)
    return;

  DEBUG_LOG("XMLTermCommands.js: Webcast: "+window.webcastFile+"\n");

  var style = "";

  if (window.xmltform1Option.showIcons == "on") {
     style += "SPAN.noicons {display: none}\n";
     style += "SPAN.icons   {display: inline}\n";
     style += "IMG.icons    {display: inline}\n";
     style += "TR.icons     {display: table-row}\n";
  }

  var exported = window.xmlterm.exportHTML(window.webcastFile, 0644, style,
                                           0, false, document.cookie);
  DEBUG_LOG("XMLTermCommands.js: exported="+exported+"\n");
}

function InitiateWebcast() {
  var field = document.getElementById('inputvalue');

  window.webcastFile = field.value ? field.value : "";
  window.webcastFile = "/var/www/html/users/xmlt.html";

  WRITE_LOG("XMLTermCommands.js: InitiateWebcast: "+window.webcastFile+"\n");

  Webcast();
  window.webcastIntervalId = window.setInterval(Webcast, 2000);
}

function TerminateWebcast() {
  window.webcastFile = null;

  if (window.webcastIntervalId) {
      window.clearInterval(window.webcastIntervalId);
      window.webcastIntervalId = null;
  }
}

function ToggleWebcast() {
  if (window.webcastIntervalId)
    TerminateWebcast();
  else
    InitiateWebcast();
}

// Insert help element displaying URL in an IFRAME before output element
// of entryNumber, or before the SESSION element if entryNumber is zero.
// Height is the height of the IFRAME in pixels.
// If URL is the null string, simply delete the help element

function ShowHelp(url, entryNumber, height) {

  if (!height) height = 120;

  DEBUG_LOG("xmlterm: ShowHelp("+url+","+entryNumber+","+height+")\n");

  if (entryNumber) {
    beforeID = "output"+entryNumber;
    helpID = "help"+entryNumber;
  } else {
    beforeID = "session";
    helpID = "help";
  }

  var beforeElement = document.getElementById(beforeID);

  if (!beforeElement) {
    DEBUG_LOG("InsertIFrame: beforeElement ID="+beforeID+"not found\n");
    return false;
  }

  var parentNode = beforeElement.parentNode;

  var helpElement = document.getElementById(helpID);
  if (helpElement) {
    // Delete help element
    parentNode.removeChild(helpElement);
    helpElement = null;
    // *NOTE* Need to flush display here to avoid black flash?
  }

  if (url.length > 0) {
    // Create new help element
    helpElement = document.createElement("div");
    helpElement.setAttribute('id', helpID);
    helpElement.setAttribute('class', 'help');

    var closeElement = document.createElement("span");
    closeElement.setAttribute('class', 'helplink');
    closeElement.appendChild(document.createTextNode("Close help frame"));
    closeElement.appendChild(document.createElement("p"));

    var iframe = document.createElement("iframe");
    iframe.setAttribute('id', helpID+'frame');
    iframe.setAttribute('class', 'helpframe');
    iframe.setAttribute('width', '100%');
    iframe.setAttribute('height', height);
    iframe.setAttribute('frameborder', '0');
    iframe.setAttribute('src', url);

    helpElement.appendChild(iframe);
    helpElement.appendChild(closeElement);

    DEBUG_LOG(helpElement);

    // Insert help element
    parentNode.insertBefore(helpElement, beforeElement);

    // NOTE: Need to do this *after* node is inserted into document
    closeElement.setAttribute('onClick', 'return ShowHelp("",'+entryNumber
                                          +');');
  }

  return false;
}

// About XMLTerm
function AboutXMLTerm() {
  DEBUG_LOG("xmlterm: AboutXMLTerm\n");

  var tipdata = document.getElementById('tipdata');
  tipdata.firstChild.data = "";

  ShowHelp('xmltermAbout.html',0,120);

  return false;
}

function Redirect() {
   window.location = window.redirectURL;
}


function GetQueryParam(paramName) {
  var url = document.location.href;

  var paramExp = new RegExp("[?&]"+paramName+"=([^&]*)");

  var matches = url.match(paramExp);

  var paramVal = "";
  if (matches && (matches.length > 1)) {
     paramVal = matches[1];
  }

  return paramVal;
}

// onLoad event handler
function LoadHandler() {
  WRITE_LOG("xmlterm: LoadHandler ... "+window.xmlterm+"\n");

  // Ugly workaround for accessing rules in stylesheet until bug 53448 is fixed
  ///var sheet = document.styleSheets[0];
  //WRITE_LOG("sheet.cssRules.length="+sheet.cssRules.length+"\n");

  ///var styleElement = (document.getElementsByTagName("style"))[0];
  ///var styleText = styleElement.firstChild.data;

  ///gStyleRuleNames = styleText.match(/\b[\w-.]+(?=\s*\{)/g);
  //WRITE_LOG("gStyleRuleNames.length="+gStyleRuleNames.length+"\n");

  //NewTip();

  // Update settings
  window.xmltform1Index  = new Object();
  window.xmltform1Option = new Object();

  window.xmltform1Index.userLevel = 2;
  window.xmltform1Option.userLevel = "advanced";

  window.xmltform1Index.showIcons = 0;
  window.xmltform1Option.showIcons = "off";

  window.xmltform1Index.windowsMode = 0;
  window.xmltform1Option.windowsMode = "off";

  if (!window.xmlterm && exportCount) {
    var redirectURL = document.location.href;
    var urlLen = redirectURL.indexOf("?");

    if (urlLen > 0) redirectURL = redirectURL.substr(0,urlLen);

    DEBUG_LOG("exportCount="+exportCount+"\n");

    var minrefresh = GetQueryParam("minrefresh");
    var maxrefresh = GetQueryParam("maxrefresh");
    var refresh = GetQueryParam("refresh");
    var id = GetQueryParam("id");

    if (minrefresh) {
       if (!maxrefresh) maxrefresh = 10*minrefresh;
       if (!refresh) refresh = minrefresh;
       if (!id) id = 0;

       if (exportCount > id*1)
         refresh = minrefresh;
       else
         refresh = 2*refresh;

       if (refresh > maxrefresh) refresh=maxrefresh;

       var refreshTime = refresh*1000;

       redirectURL += "?minrefresh="+minrefresh + "&maxrefresh="+maxrefresh +"&refresh="+refresh + "&id="+exportCount;

       DEBUG_LOG("redirectURL="+redirectURL+"\n");

       window.redirectURL = redirectURL;
       window.timeoutId = window.setTimeout(Redirect, refreshTime);
    }

    window.scrollTo(0,4000);       // Scroll to bottom of page
  }

  return false;

  // The following code fragment is skipped because the chrome takes care of
  // XMLterm initialization. This should eventually be deleted
  if (window.xmlterm) {
     // XMLTerm already initialized
     return false;
  }

  DEBUG_LOG("LoadHandler: WINDOW.ARGUMENTS="+window.arguments+"\n");

  WRITE_LOG("Trying to make an XMLTerm Shell through the component manager...\n");

  var xmltshell = Components.classes["@mozilla.org/xmlterm/xmltermshell;1"].createInstance();

  xmltshell = xmltshell.QueryInterface(Components.interfaces.mozIXMLTermShell);
  DEBUG_LOG("Interface xmltshell2 = " + xmltshell + "\n");

  if (!xmltshell) {
    WRITE_LOG("Failed to create XMLTerm shell\n");
    window.close();
    return;
  }

  // Store the XMLTerm shell in the window
  window.xmlterm = xmltshell;

  // Content window same as current window
  var contentWindow = window;

  // Initialize XMLTerm shell in content window with argvals
  window.xmlterm.init(contentWindow, "", "");

  DEBUG_LOG("LoadHandler:"+document.cookie+"\n");

  DEBUG_LOG("contentWindow="+contentWindow+"\n");
  DEBUG_LOG("document="+document+"\n");
  DEBUG_LOG("documentElement="+document.documentElement+"\n");

  // Handle resize events
  //contentWindow.addEventListener("onresize", Resize);
  contentWindow.onresize = Resize;

  // Set focus to appropriate frame
  contentWindow.focus();

  //contentWindow.xmlterm = xmlterm;

  DEBUG_LOG(contentWindow.xmlterm);

  // The following code is for testing IFRAMEs only
  DEBUG_LOG("[Main] "+window+"\n");
  DEBUG_LOG(window.screenX+", "+window.screenY+"\n");
  DEBUG_LOG(window.scrollX+", "+window.scrollY+"\n");
  DEBUG_LOG(window.pageXOffset+", "+window.pageYOffset+"\n");

  DEBUG_LOG("IFRAME checks\n");
  var iframe = document.getElementById('iframe1');

  DEBUG_LOG("iframe="+iframe+"\n");

  frames=document.frames;
  DEBUG_LOG("frames="+frames+"\n");
  DEBUG_LOG("frames.length="+frames.length+"\n");

  framewin = frames[0];

  DEBUG_LOG("framewin="+framewin+"\n");
  DEBUG_LOG("framewin.document="+framewin.document+"\n");

  DEBUG_LOG(framewin.screenX+", "+framewin.screenY+"\n");
  DEBUG_LOG(framewin.scrollX+", "+framewin.scrollY+"\n");
  DEBUG_LOG(framewin.pageXOffset+", "+framewin.pageYOffset+"\n");

  var body = framewin.document.getElementsByTagName("BODY")[0];
  DEBUG_LOG("body="+body+"\n");

  var height= body.scrollHeight;
  DEBUG_LOG("height="+height+"\n");

//        iframe.height = 800;
//        iframe.width = 700;

//        framewin.sizeToContent();

  framewin.xmltshell = xmltshell;
  DEBUG_LOG(framewin.xmltshell+"\n");

  DEBUG_LOG("xmlterm: LoadHandler completed\n");
  return false;
}

