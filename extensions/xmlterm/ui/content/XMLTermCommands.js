// XMLTerm Page Commands

// CONVENTION: All pre-defined XMLTerm Javascript functions and global
//             variables should begin with an upper-case letter.
//             This would allow them to be easily distinguished from
//             user defined functions, which should begin with a lower case
//             letter.

// Global variables
var AltWin;                   // Alternate (browser) window

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

  dump("xmlterm: NewTip "+SelectedTip+","+ranval+"\n");

  var tipdata = document.getElementById('tipdata');
  tipdata.firstChild.data = Tips[SelectedTip];

  ShowHelp("",0);

  return false;
}

// Explain tip
function ExplainTip(tipName) {
  dump("xmlterm: ExplainTip("+tipName+")\n");

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
  dump("ScrollHomeKey("+isShift+","+isControl+")\n");

  if (isControl) {
    return F1Key(isShift, 0);
  } else {
    ScrollWin(isShift,isControl).scroll(0,0);
    return false;
  }
}

// Scroll End key
function ScrollEndKey(isShift, isControl) {
  dump("ScrollEndKey("+isShift+","+isControl+")\n");

  if (isControl) {
    return F2Key(isShift, 0);
  } else {
    ScrollWin(isShift,isControl).scroll(0,99999);
    return false;
  }
}

// Scroll PageUp key
function ScrollPageUpKey(isShift, isControl) {
  dump("ScrollPageUpKey("+isShift+","+isControl+")\n");

  ScrollWin(isShift,isControl).scrollBy(0,-120);
  return false;
}

// Scroll PageDown key
function ScrollPageDownKey(isShift, isControl) {
  dump("ScrollPageDownKey("+isShift+","+isControl+")\n");

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
  dump("SetHistory("+value+")\n");
  window.xmlterm.SetHistory(value, document.cookie);
  return (false);
}

// Set prompt
function SetPrompt(value) {
  dump("SetPrompt("+value+")\n");
  window.xmlterm.SetPrompt(value, document.cookie);
  return (false);
}

// Create new XMLTerm window
function NewXMLTerm(firstcommand) {
  newwin = window.openDialog( "chrome://xmlterm/content/xmlterm.xul",
                              "xmlterm", "chrome,dialog=no,resizable",
                              firstcommand);
  dump("NewXMLTerm: "+newwin+"\n")
  return (false);
}

// Handle resize events
function Resize(event) {
  dump("Resize()\n");
  window.xmlterm.Resize();
  return (false);
}

// Form Focus (workaround for form input being transmitted to xmlterm)
function FormFocus() {
  dump("FormFocus()\n");
  window.xmlterm.IgnoreKeyPress(true, document.cookie);
  return false;
}

// Form Blur (workaround for form input being transmitted to xmlterm)
function FormBlur() {
  dump("FormBlur()\n");
  window.xmlterm.IgnoreKeyPress(false, document.cookie);
  return false;
}

// Set user level
function UpdateSettings() {

  var oldUserLevel = window.userLevel;
  window.userLevel = document.xmltform1.level.options[document.xmltform1.level.selectedIndex].value;

  var oldShowIcons = window.showIcons;
  window.showIcons = document.xmltform1.icons.options[document.xmltform1.icons.selectedIndex].value;

  window.windowsMode = document.xmltform1.windows.options[document.xmltform1.windows.selectedIndex].value;

  dump("UpdateSettings: userLevel="+window.userLevel+"\n");
  dump("UpdateSettings: windowsMode="+window.windowsMode+"\n");
  dump("UpdateSettings: showIcons="+window.showIcons+"\n");

  if (window.userLevel != oldUserLevel) {
    // Change icon display style in the style sheet

    if (window.userLevel == "advanced") {
      AlterStyle("DIV.beginner",     "display", "none");
      AlterStyle("DIV.intermediate", "display", "none");

    } else if (window.userLevel == "intermediate") {
      AlterStyle("DIV.intermediate", "display", "block");
      AlterStyle("DIV.beginner",     "display", "none");

    } else {
      AlterStyle("DIV.beginner",     "display", "block");
      AlterStyle("DIV.intermediate", "display", "block");
    }
  }

  if (window.showIcons != oldShowIcons) {
    // Change icon display style in the style sheet

    if (window.showIcons == "on") {
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
  }

  return false;
}

// Alter style in stylesheet of specified document doc, or current document
// if doc is omitted.
// ****CSS DOM IS BROKEN****; selectorText seems to be null for all rules
function AlterStyle(ruleName, propertyName, propertyValue, doc) {

  dump("AlterStyle("+ruleName+"{"+propertyName+":"+propertyValue+"})\n");

  if (!doc) doc = window.document;

  var sheet = doc.styleSheets[0];
  var r;
  for (r = 0; r < sheet.cssRules.length; r++) {
    //dump("selectorText["+r+"]="+sheet.cssRules[r].selectorText+"\n");
    //dump("cssText["+r+"]="+sheet.cssRules[r].cssText+"\n");

    if (sheet.cssRules[r].selectorText == ruleName) {
      var style = sheet.cssRules[r].style;
      //dump("style="+style.getPropertyValue(propertyName)+"\n");

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
  var succeeded = false;
  if (window.xmltbrowser) {
     dump("Load:xmltbrowser.location.href="+window.xmltbrowser.location.href+"\n");
     if (window.xmltbrowser.location.href.length) {
        window.xmltbrowser.location = url;
        succeeded = true;
     }
  }
  if (!succeeded) {
     window.xmltbrowser = window.open(url, "xmltbrowser");
  }

  // Save browser window object in global variable
  AltWin = window.xmltbrowser;

  return (false);
}

// Control display of all output elements
function DisplayAllOutput(flag) {
  var outputElements = document.getElementsByName("output");
  for (i=0; i<outputElements.length-1; i++) {
    var outputElement = outputElements[i];
    outputElement.style.display = (flag) ? "block" : "none";
  }

  var promptElements = document.getElementsByName("prompt");
  for (i=0; i<promptElements.length-1; i++) {
    promptElement = promptElements[i];
    promptElement.style.setProperty("text-decoration",
                                     (flag) ? "none" : "underline", "")
  }

  if (!flag) {
    ScrollHomeKey(0,0);
//    ScrollEndKey(0,0);
  }

  return (false);
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
//   (Following are inline or in new window depending upon window.windowsMode)
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
  dump("HandleEvent("+eventObj+","+eventType+","+targetType+","+
                   entryNumber+","+arg1+","+arg2+")\n");

  // Entry independent targets
  if (action === "textlink") {
    // Single click opens hyperlink
    // Browser-style
    dump("textlink = "+arg1+"\n");
    Load(arg1);

  } else if (targetType === "prompt") {
    // Single click on prompt expands/collapses command output
    var outputElement = document.getElementById("output"+entryNumber);
    var helpElement = document.getElementById("help"+entryNumber);
    var promptElement = document.getElementById("prompt"+entryNumber);
    //dump(promptElement.style.getPropertyValue("text-decoration"));

    if (outputElement.style.display == "none") {
      outputElement.style.display = "block";
      promptElement.style.setProperty("text-decoration","none","");

    } else {
      outputElement.style.display = "none";
      promptElement.style.setProperty("text-decoration","underline","");
      if (helpElement) {
        ShowHelp("",entryNumber);
      }
    }

  } else if (eventType === "click") {

     dump("clickCount="+eventObj.detail+"\n");

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
         //dump("textLength = "+currentCmdElement.firstChild.data.length+"\n");
         currentCommandEmpty = (currentCmdElement.firstChild.data.length == 0);

       } else {
         currentCommandEmpty = false;
       }
     }
     //dump("empty = "+currentCommandEmpty+"\n");

     if (targetType === "command") {
       if (!dblClick)
         return false;
       var commandElement = document.getElementById(targetType+entryNumber);
       var command = commandElement.firstChild.data;
       if (currentCommandEmpty) {
         window.xmlterm.SendText("\025"+command+"\n", document.cookie);
       } else {
         window.xmlterm.SendText(command, document.cookie);
       }

     } else {
       // Targets which may be qualified only for current entry

       if ((entryNumber >= 0) &&
         (window.xmlterm.currentEntryNumber != entryNumber)) {
         dump("NOT CURRENT COMMAND\n");
         return (false);
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
              (!isCurrentCommand || (window.windowsMode === "on")) ) {
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

         if (shiftClick || (window.windowsMode === "on")) {
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
         dump("send = "+sendStr+"\n");
         window.xmlterm.SendText(sendStr, document.cookie);

       } else if (action === "sendln") {
         dump("sendln = "+sendStr+"\n\n");
         window.xmlterm.SendText("\025"+sendStr+"\n", document.cookie);

       } else if (action === "createln") {
         dump("createln = "+sendStr+"\n\n");
         newwin = NewXMLTerm(sendStr+"\n");
       }
    }
  }

  return (false);
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

// Insert help element displaying URL in an IFRAME before output element
// of entryNumber, or before the SESSION element if entryNumber is zero.
// Height is the height of the IFRAME in pixels.
// If URL is the null string, simply delete the help element

function ShowHelp(url, entryNumber, height) {

  if (!height) height = 120;

  dump("xmlterm: ShowHelp("+url+","+entryNumber+","+height+")\n");

  if (entryNumber) {
    beforeID = "output"+entryNumber;
    helpID = "help"+entryNumber;
  } else {
    beforeID = "session";
    helpID = "help";
  }

  var beforeElement = document.getElementById(beforeID);

  if (!beforeElement) {
    dump("InsertIFrame: beforeElement ID="+beforeID+"not found\n");
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
    //closeElement.appendChild(document.createElement("p"));

    var iframe = document.createElement("iframe");
    iframe.setAttribute('id', helpID+'frame');
    iframe.setAttribute('class', 'helpframe');
    iframe.setAttribute('width', '100%');
    iframe.setAttribute('height', height);
    iframe.setAttribute('frameborder', '0');
    iframe.setAttribute('src', url);

    helpElement.appendChild(iframe);
    helpElement.appendChild(closeElement);

    dump(helpElement);

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
  dump("xmlterm: AboutXMLTerm\n");

  var tipdata = document.getElementById('tipdata');
  tipdata.firstChild.data = "";

  ShowHelp('xmltermAbout.html',0,120);

  return false;
}

// onLoad event handler
function LoadHandler() {
  dump("xmlterm: LoadHandler ... "+window.xmlterm+"\n");

  // Update settings
  UpdateSettings();

  NewTip();

  return false;

  // The following code fragment is skipped because the chrome takes care of
  // XMLterm initialization. This should eventually be deleted
  if (window.xmlterm) {
     // XMLTerm already initialized
     return false;
  }

  dump("LoadHandler: WINDOW.ARGUMENTS="+window.arguments+"\n");

  dump("Trying to make an XMLTerm Shell through the component manager...\n");

  var xmltshell = Components.classes["@mozilla.org/xmlterm/xmltermshell;1"].createInstance();

  dump("Interface xmltshell1 = " + xmltshell + "\n");

  xmltshell = xmltshell.QueryInterface(Components.interfaces.mozIXMLTermShell);
  dump("Interface xmltshell2 = " + xmltshell + "\n");

  if (!xmltshell) {
    dump("Failed to create XMLTerm shell\n");
    window.close();
    return;
  }

  // Store the XMLTerm shell in the window
  window.xmlterm = xmltshell;

  // Content window same as current window
  var contentWindow = window;

  // Initialize XMLTerm shell in content window with argvals
  window.xmlterm.Init(contentWindow, "", "");

  //dump("LoadHandler:"+document.cookie+"\n");

  dump("contentWindow="+contentWindow+"\n");
  dump("document="+document+"\n");
  dump("documentElement="+document.documentElement+"\n");

  // Handle resize events
  //contentWindow.addEventListener("onresize", Resize);
  contentWindow.onresize = Resize;

  // Set focus to appropriate frame
  contentWindow.focus();

  //contentWindow.xmlterm = xmlterm;

  //dump(contentWindow.xmlterm);

  // The following code is for testing IFRAMEs only
  dump("[Main] "+window+"\n");
  dump(window.screenX+", "+window.screenY+"\n");
  dump(window.scrollX+", "+window.scrollY+"\n");
  dump(window.pageXOffset+", "+window.pageYOffset+"\n");

  dump("IFRAME checks\n");
  var iframe = document.getElementById('iframe1');

  dump("iframe="+iframe+"\n");

  frames=document.frames;
  dump("frames="+frames+"\n");
  dump("frames.length="+frames.length+"\n");

  framewin = frames[0];

  dump("framewin="+framewin+"\n");
  dump("framewin.document="+framewin.document+"\n");

  dump(framewin.screenX+", "+framewin.screenY+"\n");
  dump(framewin.scrollX+", "+framewin.scrollY+"\n");
  dump(framewin.pageXOffset+", "+framewin.pageYOffset+"\n");

  var body = framewin.document.getElementsByTagName("BODY")[0];
  dump("body="+body+"\n");

  var height= body.scrollHeight;
  dump("height="+height+"\n");

//        iframe.height = 800;
//        iframe.width = 700;

//        framewin.sizeToContent();

  framewin.xmltshell = xmltshell;
  dump(framewin.xmltshell+"\n");

  dump("xmlterm: LoadHandler completed\n");
  return (false);
}

