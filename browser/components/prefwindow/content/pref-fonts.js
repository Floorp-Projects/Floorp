# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#    Gervase Markham <gerv@gerv.net>
#    Tuukka Tolvanen <tt@lament.cjb.net>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

try
  {
    var fontList = Components.classes["@mozilla.org/gfx/fontlist;1"].createInstance();
    if (fontList)
      fontList = fontList.QueryInterface(Components.interfaces.nsIFontList);

    var pref = Components.classes["@mozilla.org/preferences;1"].getService( Components.interfaces.nsIPref );
  }
catch(e)
  {
    dump("failed to get font list or pref object: "+e+" in pref-fonts.js\n");
  }

var fontTypes   = ["serif", "sans-serif", "monospace"];
var variableSize, fixedSize, minSize, languageList;
var languageData = [];
var currentLanguage;
var gPrefutilitiesBundle;

// manual data retrieval function for PrefWindow
function GetFields()
  {
    var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-fonts.xul"];

    // store data for language independent widgets
    var lists = ["selectLangs", "proportionalFont"];
    for( var i = 0; i < lists.length; i++ )
      {
        if( !( "dataEls" in dataObject ) )
          dataObject.dataEls = [];
        dataObject.dataEls[ lists[i] ] = [];
        dataObject.dataEls[ lists[i] ].value = document.getElementById( lists[i] ).value;
      }

   dataObject.defaultFont = document.getElementById( "proportionalFont" ).value;
   dataObject.fontDPI = document.getElementById( "screenResolution" ).value;
   dataObject.useMyFonts = document.getElementById( "useMyFonts" ).checked ? 1 : 0;

   var items = ["foregroundText", "background", "unvisitedLinks", "visitedLinks", "browserUseSystemColors", "browserUnderlineAnchors", "useMyColors", "useMyFonts"];
   for (i = 0; i < items.length; ++i) {
     dataObject.dataEls[items[i]] = [];
   }
   dataObject.dataEls["foregroundText"].value = document.getElementById("foregroundtextmenu").color;
   dataObject.dataEls["background"].value = document.getElementById("backgroundmenu").color;
   dataObject.dataEls["unvisitedLinks"].value = document.getElementById("unvisitedlinkmenu").color;
   dataObject.dataEls["visitedLinks"].value = document.getElementById("visitedlinkmenu").color;

   dataObject.dataEls["browserUseSystemColors"].checked = document.getElementById("browserUseSystemColors").checked;
   dataObject.dataEls["browserUnderlineAnchors"].checked = document.getElementById("browserUnderlineAnchors").checked;
   dataObject.dataEls["useMyColors"].checked = document.getElementById("useMyColors").checked;
   dataObject.dataEls["useMyFonts"].checked = document.getElementById("useMyFonts").checked;

    // save current state for language dependent fields and store
    saveState();
    dataObject.languageData = languageData;

    return dataObject;
  }

// manual data setting function for PrefWindow
function SetFields( aDataObject )
  {
    languageData = "languageData" in aDataObject ? aDataObject.languageData : languageData ;
    currentLanguage = "currentLanguage" in aDataObject ? aDataObject.currentLanguage : null ;

    var lists = ["selectLangs", "proportionalFont"];
    var prefvalue;
    
    for( var i = 0; i < lists.length; i++ )
      {
        var element = document.getElementById( lists[i] );
        if( "dataEls" in aDataObject )
          {
            element.selectedItem = element.getElementsByAttribute( "value", aDataObject.dataEls[ lists[i] ].value )[0];
          }
        else
          {
            var prefstring = element.getAttribute( "prefstring" );
            var preftype = element.getAttribute( "preftype" );
            if( prefstring && preftype )
              {
                prefvalue = parent.hPrefWindow.getPref( preftype, prefstring );
                element.selectedItem = element.getElementsByAttribute( "value", prefvalue )[0];
              }
          }
      }

    var screenResolution = document.getElementById( "screenResolution" );
    var resolution;
    
    if( "fontDPI" in aDataObject )
      {
        resolution = aDataObject.fontDPI;
      }
    else
      {
        prefvalue = parent.hPrefWindow.getPref( "int", "browser.display.screen_resolution" );
        if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
            resolution = prefvalue;
        else
            resolution = 96; // If it all goes horribly wrong, fall back on 96.
      }
    
    setResolution( resolution );
    
    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.screen_resolution" ) ) {
        screenResolution.disabled = true;
    }

    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.use_document_fonts" ) ) {
        useMyFontsCheckbox.disabled = true;
    }
   if ("dataEls" in aDataObject) {
     document.getElementById("foregroundtextmenu").color = aDataObject.dataEls["foregroundText"].value
     document.getElementById("backgroundmenu").color = aDataObject.dataEls["background"].value;
     document.getElementById("unvisitedlinkmenu").color = aDataObject.dataEls["unvisitedLinks"].value;
     document.getElementById("visitedlinkmenu").color = aDataObject.dataEls["visitedLinks"].value;
 
     document.getElementById("browserUseSystemColors").checked = aDataObject.dataEls["browserUseSystemColors"].checked;
     document.getElementById("browserUnderlineAnchors").checked = aDataObject.dataEls["browserUnderlineAnchors"].checked;
     document.getElementById("useMyColors").checked = aDataObject.dataEls["useMyColors"].checked;
     document.getElementById("useMyFonts").checked = aDataObject.dataEls["useMyFonts"].checked;
   }
   else {
     var elt, prefstring;
     var checkboxes = ["browserUseSystemColors", "browserUnderlineAnchors"];
     for (i = 0; i < checkboxes.length; ++i) {
       elt = document.getElementById(checkboxes[i]);
       prefstring = elt.getAttribute( "prefstring" );
       if( prefstring  ) {
         prefvalue = parent.hPrefWindow.getPref( "bool", prefstring );
         elt.checked = prefvalue;
       }
     }
     var colors = ["foregroundtextmenu", "backgroundmenu", "unvisitedlinkmenu", "visitedlinkmenu"];
     for (i = 0; i < colors.length; ++i) {
       elt = document.getElementById(colors[i]);
       prefstring = elt.nextSibling.getAttribute("prefstring");
       if (prefstring) {
         prefvalue = parent.hPrefWindow.getPref("color", prefstring);
         elt.color = prefvalue;
       }
     }
     var useDocColors = parent.hPrefWindow.getPref("bool", "browser.display.use_document_colors");
     document.getElementById("useMyColors").checked = !useDocColors;
     var useDocFonts = parent.hPrefWindow.getPref("int", "browser.display.use_document_fonts");
     document.getElementById("useMyFonts").checked = !useDocFonts;

   }
}

function Startup()
  {
    variableSize = document.getElementById( "sizeVar" );
    fixedSize    = document.getElementById( "sizeMono" );
    minSize      = document.getElementById( "minSize" );
    languageList = document.getElementById( "selectLangs" );

    gPrefutilitiesBundle = document.getElementById("bundle_prefutilities");

    // register our ok callback function
    parent.hPrefWindow.registerOKCallbackFunc( saveFontPrefs );

    // eventually we should detect the default language and select it by default
    selectLanguage();
    
    // Allow user to ask the OS for a DPI if we are under X or OS/2
    if ((navigator.appVersion.indexOf("X11") != -1) || (navigator.appVersion.indexOf("OS/2") != -1))
      {
         document.getElementById( "systemResolution" ).removeAttribute( "hidden" ); 
      }
    
    // Set up the labels for the standard issue resolutions
    var resolution;
    resolution = document.getElementById( "screenResolution" );

    // Set an attribute on the selected resolution item so we can fall back on
    // it if an invalid selection is made (select "Other...", hit Cancel)
    resolution.selectedItem.setAttribute("current", "true");

    var defaultResolution;
    var otherResolution;

    // On OS/2, 120 is the default system resolution.
    // 96 is valid, but used only for for 640x480.
    if (navigator.appVersion.indexOf("OS/2") != -1)
      {
        defaultResolution = "120";
        otherResolution = "96";
        document.getElementById( "arbitraryResolution" ).setAttribute( "hidden", "true" ); 
        document.getElementById( "resolutionSeparator" ).setAttribute( "hidden", "true" ); 
      } else {
        defaultResolution = "96";
        otherResolution = "72";
      }

    var dpi = resolution.getAttribute( "dpi" );
    resolution = document.getElementById( "defaultResolution" );
    resolution.setAttribute( "value", defaultResolution );
    resolution.setAttribute( "label", dpi.replace(/\$val/, defaultResolution ) );
    resolution = document.getElementById( "otherResolution" );
    resolution.setAttribute( "value", otherResolution );
    resolution.setAttribute( "label", dpi.replace(/\$val/, otherResolution ) );

    // Get the pref and set up the dialog appropriately. Startup is called
    // after SetFields so we can't rely on that call to do the business.
    var prefvalue = parent.hPrefWindow.getPref( "int", "browser.display.screen_resolution" );
    if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
        resolution = prefvalue;
    else
        resolution = 96; // If it all goes horribly wrong, fall back on 96.
    
    setResolution( resolution );

    // This prefstring is a contrived pref whose sole purpose is to lock some
    // elements in this panel.  The value of the pref is not used and does not matter.
    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.languageList" ) ) {
      disableAllFontElements();
    }
  }

function listElement( aListID )
  {
    this.listElement = document.getElementById( aListID );
  }

listElement.prototype =
  {
    clearList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.listElement.removeChild( this.listElement.firstChild );
        },

    appendString:
      function ( aString )
        {
          var menuItemNode = document.createElement( "menuitem" );
          if( menuItemNode )
            {
              menuItemNode.setAttribute( "label", aString );
              this.listElement.firstChild.appendChild( menuItemNode );
            }
        },

    appendFontNames: 
      function ( aDataObject ) 
        { 
          var popupNode = document.createElement( "menupopup" ); 
          var strDefaultFontFace = "";
          var fontName;
          while (aDataObject.hasMoreElements()) {
            fontName = aDataObject.getNext();
            fontName = fontName.QueryInterface(Components.interfaces.nsISupportsString);
            var fontNameStr = fontName.toString();
            if (strDefaultFontFace == "")
              strDefaultFontFace = fontNameStr;
            var itemNode = document.createElement( "menuitem" );
            itemNode.setAttribute( "value", fontNameStr );
            itemNode.setAttribute( "label", fontNameStr );
            popupNode.appendChild( itemNode );
          }
          if (strDefaultFontFace != "") {
            this.listElement.removeAttribute( "disabled" );
          } else {
            this.listElement.setAttribute( "value", strDefaultFontFace );
            this.listElement.setAttribute( "label",
                                    gPrefutilitiesBundle.getString("nofontsforlang") );
            this.listElement.setAttribute( "disabled", "true" );
          }
          this.listElement.appendChild( popupNode ); 
          return strDefaultFontFace;
        } 
  };

function saveFontPrefs()
  {
    // if saveState function is available, assume can call it.
    // why is this extra qualification required?!!!!
    if( top.hPrefWindow.wsm.contentArea.saveState )
      {
        saveState();
        parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-fonts.xul"] = GetFields();
      }

    // saving font prefs
    var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-fonts.xul"];
    var pref = parent.hPrefWindow.pref;
    for( var language in dataObject.languageData )
      {
        for( var type in dataObject.languageData[language].types )
          {
            var fontPrefString = "font.name." + type + "." + language;
            var currValue = "";
            try
              {
                currValue = pref.CopyUnicharPref( fontPrefString );
              }
            catch(e)
              {
              }
            if( currValue != dataObject.languageData[language].types[type] )
              pref.SetUnicharPref( fontPrefString, dataObject.languageData[language].types[type] );
          }
        var variableSizePref = "font.size.variable." + language;
        var fixedSizePref = "font.size.fixed." + language;
        var minSizePref = "font.minimum-size." + language;
        var currVariableSize = 12, currFixedSize = 12, minSizeVal = 0;
        try
          {
            currVariableSize = pref.GetIntPref( variableSizePref );
            currFixedSize = pref.GetIntPref( fixedSizePref );
            minSizeVal = pref.GetIntPref( minSizePref );
          }
        catch(e)
          {
          }
        if( currVariableSize != dataObject.languageData[language].variableSize )
          pref.SetIntPref( variableSizePref, dataObject.languageData[language].variableSize );
        if( currFixedSize != dataObject.languageData[language].fixedSize )
          pref.SetIntPref( fixedSizePref, dataObject.languageData[language].fixedSize );
        if ( minSizeVal != dataObject.languageData[language].minSize ) {
          pref.SetIntPref ( minSizePref, dataObject.languageData[language].minSize );
        }
      }

    // font scaling
    var fontDPI       = parseInt( dataObject.fontDPI );
    var myFonts = dataObject.dataEls["useMyFonts"].checked;
    var defaultFont   = dataObject.defaultFont;
    var myColors = dataObject.dataEls["useMyColors"].checked;
    try
      {
        var currDPI = pref.GetIntPref( "browser.display.screen_resolution" );
        var currFonts = pref.GetIntPref( "browser.display.use_document_fonts" );
        var currColors = pref.GetBoolPref("browser.display.use_document_colors");
        var currDefault = pref.CopyUnicharPref( "font.default" );
      }
    catch(e)
      {
      }
    if( currDPI != fontDPI )
      pref.SetIntPref( "browser.display.screen_resolution", fontDPI );
    if( currFonts == myFonts )
      pref.SetIntPref( "browser.display.use_document_fonts", !myFonts );
    if( currDefault != defaultFont )
      {
        pref.SetUnicharPref( "font.default", defaultFont );
      }
    if (currColors == myColors)
      pref.SetBoolPref("browser.display.use_document_colors", !myColors);

    var items = ["foregroundText", "background", "unvisitedLinks", "visitedLinks"];
    var prefs = ["browser.display.foreground_color", "browser.display.background_color",
                 "browser.anchor_color", "browser.visited_color"];
    var prefvalue;
    for (var i = 0; i < items.length; ++i) {
      prefvalue = dataObject.dataEls[items[i]].value;
      pref.SetUnicharPref(prefs[i], prefvalue)
    }
    items = ["browserUseSystemColors", "browserUnderlineAnchors"];
    prefs = ["browser.display.use_system_colors", "browser.underline_anchors"];
    for (var i = 0; i < items.length; ++i) {
      prefvalue = dataObject.dataEls[items[i]].checked;
      pref.SetBoolPref(prefs[i], prefvalue)
    }
  }

function saveState()
  {
    for( var i = 0; i < fontTypes.length; i++ )
      {
        // preliminary initialisation
        if( currentLanguage && !( currentLanguage in languageData ) )
          languageData[currentLanguage] = [];
        if( currentLanguage && !( "types" in languageData[currentLanguage] ) )
          languageData[currentLanguage].types = [];
        // save data for the previous language
        if( currentLanguage && currentLanguage in languageData &&
            "types" in languageData[currentLanguage] )
          languageData[currentLanguage].types[fontTypes[i]] = document.getElementById( fontTypes[i] ).value;
      }

    if( currentLanguage && currentLanguage in languageData &&
        "types" in languageData[currentLanguage] )
      {
        languageData[currentLanguage].variableSize = parseInt( variableSize.value );
        languageData[currentLanguage].fixedSize = parseInt( fixedSize.value );
        languageData[currentLanguage].minSize = parseInt( minSize.value );
      }
  }

// Selects size (or the nearest entry that exists in the list)
// in the menulist minSize
function minSizeSelect(size)
  {
    var items = minSize.getElementsByAttribute( "value", size );
    if (items.length > 0)
      minSize.selectedItem = items[0];
    else if (size < 6)
      minSizeSelect(6);
    else if (size > 24)
      minSizeSelect(24);
    else
      minSizeSelect(size - 1);
  }

function selectLanguage()
  {
    // save current state
    saveState();

    if( !currentLanguage )
      currentLanguage = languageList.value;

    for( var i = 0; i < fontTypes.length; i++ )
      {
        // build and populate the font list for the newly chosen font type
        var fontEnumerator = fontList.availableFonts(languageList.value, fontTypes[i]);
        var selectElement = new listElement( fontTypes[i] );
        selectElement.clearList();
        var strDefaultFontFace = selectElement.appendFontNames(fontEnumerator);
        //the first font face name returned by the enumerator is our last resort
        var defaultListSelection = selectElement.listElement.getElementsByAttribute( "value", strDefaultFontFace)[0];
        var selectedItem;

        //fall-back initialization values (first font face list entry)
        defaultListSelection = strDefaultFontFace ? selectElement.listElement.getElementsByAttribute( "value", strDefaultFontFace)[0] : null;

        if( languageList.value in languageData )
          {
            // data exists for this language, pre-select items based on this information
            var dataElements = selectElement.listElement.getElementsByAttribute( "value", languageData[languageList.value].types[fontTypes[i]] );
            selectedItem = dataElements.length ? dataElements[0] : defaultListSelection;

            minSizeSelect(languageData[languageList.value].minSize);
            if (strDefaultFontFace)
              {
                selectElement.listElement.selectedItem = selectedItem;
                variableSize.removeAttribute("disabled");
                fixedSize.removeAttribute("disabled");
                minSize.removeAttribute("disabled");
                variableSize.selectedItem = variableSize.getElementsByAttribute( "value", languageData[languageList.value].variableSize )[0];
                fixedSize.selectedItem = fixedSize.getElementsByAttribute( "value", languageData[languageList.value].fixedSize )[0];
              }
            else
              {
                variableSize.setAttribute("disabled","true");
                fixedSize.setAttribute("disabled","true");
                minSize.setAttribute("disabled","true");
              }
          }
        else
          {

            if (strDefaultFontFace) {
                //initialze pref panel only if font faces are available for this language family

                var selectVal;

                try {
                    var fontPrefString = "font.name." + fontTypes[i] + "." + languageList.value;
                    selectVal   = parent.hPrefWindow.pref.CopyUnicharPref( fontPrefString );
                    var dataEls = selectElement.listElement.getElementsByAttribute( "value", selectVal );

                    // we need to honor name-list in case name is unavailable 
                    if (!dataEls.length) {
                        var fontListPrefString = "font.name-list." + fontTypes[i] + "." + languageList.value;
                        var nameList   = parent.hPrefWindow.pref.CopyUnicharPref( fontListPrefString );
                        var fontNames = nameList.split(",");
                        var stripWhitespace = /^\s*(.*)\s*$/;

                        for (j = 0; j < fontNames.length; j++) {
                          selectVal = fontNames[j].replace(stripWhitespace, "$1");
                          dataEls = selectElement.listElement.getElementsByAttribute("value", selectVal);
                          if (dataEls.length)  
                            break;  // exit loop if we find one
                        }
                    }                     

                    selectedItem = dataEls.length ? dataEls[0] : defaultListSelection;
                    if (!dataEls.length) 
                      selectedVal = strDefaultFontFace;
                }
                catch(e) {
                    //always initialize: fall-back to default values
                    selectVal       = strDefaultFontFace;
                    selectedItem    = defaultListSelection;
                }

                selectElement.listElement.selectedItem = selectedItem;

                variableSize.removeAttribute("disabled");
                fixedSize.removeAttribute("disabled");
                minSize.removeAttribute("disabled");


                try {
                    var variableSizePref = "font.size.variable." + languageList.value;
                    var fixedSizePref = "font.size.fixed." + languageList.value;

                    var sizeVarVal = parent.hPrefWindow.pref.GetIntPref( variableSizePref );
                    var sizeFixedVal = parent.hPrefWindow.pref.GetIntPref( fixedSizePref );

                    variableSize.selectedItem = variableSize.getElementsByAttribute( "value", sizeVarVal )[0];

                    fixedSize.selectedItem = fixedSize.getElementsByAttribute( "value", sizeFixedVal )[0];
                }

                catch(e) {
                    //font size lists can simply deafult to the first entry
                }
                var minSizeVal = 0;
                try {
                  var minSizePref = "font.minimum-size." + languageList.value;
                  minSizeVal = pref.GetIntPref( minSizePref );
                }
                catch(e) {}
                minSizeSelect( minSizeVal );

            }
            else
            {
                //disable otherwise                
                variableSize.setAttribute("disabled","true");
                fixedSize.setAttribute("disabled","true");
                minSize.setAttribute("disabled","true");
                minSizeSelect(0);
            }
          }
      }
    currentLanguage = languageList.value;
  }

function changeScreenResolution()
  {
    var screenResolution = document.getElementById("screenResolution");
    var userResolution = document.getElementById("userResolution");

    var previousSelection = screenResolution.getElementsByAttribute("current", "true")[0];

    if (screenResolution.value == "other")
      {
        // If the user selects "Other..." we bring up the calibrate screen dialog
        var rv = { newdpi : 0 };
        var calscreen = window.openDialog("chrome://browser/content/pref/pref-calibrate-screen.xul", 
                                      "_blank", 
                                      "modal,chrome,centerscreen,resizable=no,titlebar",
                                      rv);
        if (rv.newdpi != -1) 
          {
            // They have entered values, and we have a DPI value back
            var dpi = screenResolution.getAttribute( "dpi" );
            setResolution ( rv.newdpi );

            previousSelection.removeAttribute("current");
            screenResolution.selectedItem.setAttribute("current", "true");
          }
        else
          {
            // They've cancelled. We can't leave "Other..." selected, so...
            // we re-select the previously selected item.
            screenResolution.selectedItem = previousSelection;
          }
      }
    else if (!(screenResolution.value == userResolution.value))
      {
        // User has selected one of the hard-coded resolutions
        userResolution.setAttribute("hidden", "true");

        previousSelection.removeAttribute("current");
        screenResolution.selectedItem.setAttribute("current", "true");
      }
  }

function setResolution( resolution )
  {
    // Given a number, if it's equal to a hard-coded resolution we use that,
    // otherwise we set the userResolution field.
    var screenResolution = document.getElementById( "screenResolution" );
    var userResolution = document.getElementById( "userResolution" );

    var items = screenResolution.getElementsByAttribute( "value", resolution );
    if (items.length)
      {
        // If it's one of the hard-coded values, we'll select it directly 
        screenResolution.selectedItem = items[0];
        userResolution.setAttribute( "hidden", "true" );
      }   
    else
      {
        // Otherwise we need to set up the userResolution field
        var dpi = screenResolution.getAttribute( "dpi" );
        userResolution.setAttribute( "value", resolution );
        userResolution.setAttribute( "label", dpi.replace(/\$val/, resolution) );
        userResolution.removeAttribute( "hidden" );
        screenResolution.selectedItem = userResolution;   
      }
  }
  
// "Calibrate screen" dialog code

function Init()
  {
      sizeToContent();
      doSetOKCancel(onOK, onCancel);
      document.getElementById("horizSize").focus();
  }
  
function onOK()
  {
    // Get value from the dialog to work out dpi
    var horizSize = parseFloat(document.getElementById("horizSize").value);
    var units = document.getElementById("units").value;
  
    if (!horizSize || horizSize < 0)
      {
        // We can't calculate anything without a proper value
        window.arguments[0].newdpi = -1;
        return true;
      }
      
    // Convert centimetres to inches.
    // The magic number is allowed because it's a fundamental constant :-)
    if (units === "centimetres")
      {
        horizSize /= 2.54;
      }
  
    // These shouldn't change, but you can't be too careful.
    var horizBarLengthPx = document.getElementById("horizRuler").boxObject.width;
  
    var horizDPI = parseInt(horizBarLengthPx) / horizSize;
  
    // Average the two <shrug>.
    window.arguments[0].newdpi = Math.round(horizDPI);
  
    return true;
  }

function onCancel()
  {
      // We return -1 to show that no value has been given.
      window.arguments[0].newdpi = -1;
      return true;
  }

// disable font items, but not the useMyFonts checkbox nor the resolution
// menulist
function disableAllFontElements()
  {
      var doc_ids = [ "selectLangs", "proportionalFont",
                      "sizeVar", "serif", "sans-serif",
                      "sizeMono", "minSize" ];
      for (i=0; i<doc_ids.length; i++) {
          element = document.getElementById( doc_ids[i] );
          element.disabled = true;
      }
  }

