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
    var pref = Components.classes["@mozilla.org/preferences;1"].getService( Components.interfaces.nsIPref );
  }
catch(e)
  {
    dump("failed to get font list or pref object: "+e+" in pref-fonts.js\n");
  }

var fontEnumerator = null;
var globalFonts = null;
var fontTypes   = ["serif", "sans-serif", "monospace"];
var variableSize, fixedSize, minSize, languageList;
var languageData = [];
var currentLanguage;
var gPrefutilitiesBundle;

// manual data retrieval function for PrefWindow
function GetFields()
  {
    var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-fonts.xul"];

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
   dataObject.useDocFonts = document.getElementById( "browserUseDocumentFonts" ).checked ? 1 : 0;

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

    var useDocFontsCheckbox = document.getElementById( "browserUseDocumentFonts" );
    if( "useDocFonts" in aDataObject && aDataObject.useDocFonts != undefined )
      useDocFontsCheckbox.checked = aDataObject.useDocFonts ? true : false;
    else
      {
        prefvalue = parent.hPrefWindow.getPref( "int", "browser.display.use_document_fonts" );
        if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
          useDocFontsCheckbox.checked = prefvalue ? true : false ;
      }
    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.use_document_fonts" ) ) {
        useDocFontsCheckbox.disabled = true;
    }

    return true;
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

function getFontEnumerator()
  {
    if (!fontEnumerator)
      {
        fontEnumerator = Components.classes["@mozilla.org/gfx/fontenumerator;1"]
                        .createInstance()
                        .QueryInterface(Components.interfaces.nsIFontEnumerator);
      }
    return fontEnumerator;
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

    appendFontNames: 
      function ( aLanguage, aFontType )
        {
          var i;
          var defaultFont = null;
          var count = { value: 0 };
          var fonts = getFontEnumerator().EnumerateFonts( aLanguage, aFontType, count );
          if (fonts.length > 0)
            {
              defaultFont = getFontEnumerator().getDefaultFont( aLanguage, aFontType );
            }
          else
            {
              // if no specific fonts, relax 'aFontType' and try to get other
              // fonts for this language so that we can group them on top
              fonts = getFontEnumerator().EnumerateFonts( aLanguage, "", count );
              if (fonts.length > 0)
                {
                  defaultFont = getFontEnumerator().getDefaultFont( aLanguage, "" );
                }
            }

          var itemNode = null;
          var separatorNode = null;
          var popupNode = document.createElement( "menupopup" ); 

          if (fonts.length > 0)
            {
              // always put the default font at the front of the list
              if (defaultFont)
                {
                  var label = gPrefutilitiesBundle
                    .getString("labelDefaultFont")
                    .replace(/%font_family%/, defaultFont);
                  itemNode = document.createElement( "menuitem" );
                  itemNode.setAttribute( "label", label );
                  itemNode.setAttribute( "value", "" ); // special blank value
                  popupNode.appendChild( itemNode );

                  separatorNode = document.createElement( "menuseparator" );
                  popupNode.appendChild( separatorNode );
                }

              for (i = 0; i < fonts.length; i++)
                {
                  itemNode = document.createElement( "menuitem" );
                  itemNode.setAttribute( "value", fonts[i] );
                  itemNode.setAttribute( "label", fonts[i] );
                  popupNode.appendChild( itemNode );
                }
            }

            // get all the fonts to complete the font lists
            if (!globalFonts)
              {
                globalFonts = getFontEnumerator().EnumerateAllFonts( count );
              }

          // since the lists are sorted, we can get unique entries by just walking
          // both lists linearly side-by-side, skipping those values already in
          // the popup list
          if (globalFonts.length > fonts.length)
            {
              var menuItem = separatorNode ? separatorNode.nextSibling : popupNode.firstChild; 
              var menuValue = menuItem ? menuItem.getAttribute( "value" ) : null;

              separatorNode = document.createElement( "menuseparator" );
              popupNode.appendChild( separatorNode );

              for (i = 0; i < globalFonts.length; i++)
                {
                  if (globalFonts[i] != menuValue)
                    {
                      itemNode = document.createElement( "menuitem" );
                      itemNode.setAttribute( "value", globalFonts[i] );
                      itemNode.setAttribute( "label", globalFonts[i] );
                      popupNode.appendChild( itemNode );
                    }
                  else
                    {
                      menuItem = menuItem.nextSibling;
                      menuValue = menuItem ? menuItem.getAttribute( "value" ) : null;
                    }
                }
            }

          this.listElement.appendChild( popupNode ); 

          return popupNode.firstChild;
        } 
  };

function lazyAppendFontNames( i )
  {
     // schedule the build of the next font list
     if (i+1 < fontTypes.length)
       {
         window.setTimeout(lazyAppendFontNames, 100, i+1);
       }

     // now build and populate the fonts for the requested font type
     var defaultItem = null;
     var selectElement = new listElement( fontTypes[i] );
     selectElement.clearList();
     try
       {
         defaultItem = selectElement.appendFontNames( languageList.value, fontTypes[i] );
       }
     catch(e) {
         dump("pref-fonts.js: " + e + "\nFailed to build the font list for " + fontTypes[i] + "\n");
         return;
       }

     // now set the selected font item for the drop down list

     if (!defaultItem)
       return; // nothing to select, so no need to bother

     // the item returned by default is our last resort fall-back
     var selectedItem = defaultItem;
     if( languageList.value in languageData )
       {
         // data exists for this language, pre-select items based on this information
         var dataVal = languageData[languageList.value].types[fontTypes[i]];
         if (!dataVal.length) // special blank means the default
           {
             selectedItem = defaultItem;
           }
         else
           {
             var dataEls = selectElement.listElement.getElementsByAttribute( "value", dataVal );
             selectedItem = dataEls.item(0) ? dataEls[0] : defaultItem;
           }
       }
     else
       {
         try
           {
             var fontPrefString = "font.name." + fontTypes[i] + "." + languageList.value;
             var selectVal = parent.hPrefWindow.pref.CopyUnicharPref( fontPrefString );
             var dataEls = selectElement.listElement.getElementsByAttribute( "value", selectVal );

             // we need to honor name-list in case name is unavailable 
             if (!dataEls.item(0)) {
                 var fontListPrefString = "font.name-list." + fontTypes[i] + "." + languageList.value;
                 var nameList = parent.hPrefWindow.pref.CopyUnicharPref( fontListPrefString );
                 var fontNames = nameList.split(",");
                 var stripWhitespace = /^\s*(.*)\s*$/;

                 for (j = 0; j < fontNames.length; j++) {
                   selectVal = fontNames[j].replace(stripWhitespace, "$1");
                   dataEls = selectElement.listElement.getElementsByAttribute("value", selectVal);
                   if (dataEls.item(0))  
                     break;  // exit loop if we find one
                 }
             }
             selectedItem = dataEls.item(0) ? dataEls[0] : defaultItem;
           }
         catch(e) {
             selectedItem = defaultItem;
           }
       }

     selectElement.listElement.selectedItem = selectedItem;
     selectElement.listElement.removeAttribute( "disabled" );
  }

function saveFontPrefs()
  {
    // if saveState function is available, assume can call it.
    // why is this extra qualification required?!!!!
    if( top.hPrefWindow.wsm.contentArea.saveState )
      {
        saveState();
        parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-fonts.xul"] = GetFields();
      }

    // saving font prefs
    var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-fonts.xul"];
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

            var dataValue = dataObject.languageData[language].types[type];
            if( currValue != dataValue )
              {
                if (dataValue)
                  {
                    pref.SetUnicharPref( fontPrefString, dataValue );
                  }
                else
                  {
                    // A font name can't be blank. The special blank means the default.
                    // Unset the pref entirely, letting Gfx to decide. GfxXft will use what
                    // Xft says, whereas GfxWin and others will use the built-in settings
                    // that are shipped for font.name and font.name-list.
                    try
                      {
                        pref.clearUserPref( fontPrefString );
                      }
                    catch(e)
                      {
                      }
                  }
              }
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
    var documentFonts = dataObject.useDocFonts;
    var defaultFont   = dataObject.defaultFont;

    try
      {
        var currDPI = pref.GetIntPref( "browser.display.screen_resolution" );
        var currFonts = pref.GetIntPref( "browser.display.use_document_fonts" );
        var currDefault = pref.CopyUnicharPref( "font.default" );
      }
    catch(e)
      {
      }
    if( currDPI != fontDPI )
      pref.SetIntPref( "browser.display.screen_resolution", fontDPI );
    if( currFonts != documentFonts )
      pref.SetIntPref( "browser.display.use_document_fonts", documentFonts );
    if( currDefault != defaultFont )
      {
        pref.SetUnicharPref( "font.default", defaultFont );
      }
    // save off the language information...

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
    if (items.item(0))
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
    else if( currentLanguage == languageList.value )
      return; // same as before, nothing changed

    // lazily populate the successive font lists at 100ms intervals.
    // (Note: the third parameter to setTimeout() is going to be
    // passed as argument to the callback function.)
    window.setTimeout(lazyAppendFontNames, 100, 0);

    // in the meantime, disable the menu lists
    for( var i = 0; i < fontTypes.length; i++ )
      {
        var listElement = document.getElementById( fontTypes[i] );
        listElement.setAttribute( "value", "" );
        listElement.setAttribute( "label", "" );
        listElement.setAttribute( "disabled", "true" );
      }

    // and set the font sizes
    try
      {
        var variableSizePref = "font.size.variable." + languageList.value;
        var sizeVarVal = parent.hPrefWindow.pref.GetIntPref( variableSizePref );
        variableSize.selectedItem = variableSize.getElementsByAttribute( "value", sizeVarVal )[0];

        var fixedSizePref = "font.size.fixed." + languageList.value;
        var sizeFixedVal = parent.hPrefWindow.pref.GetIntPref( fixedSizePref );
        fixedSize.selectedItem = fixedSize.getElementsByAttribute( "value", sizeFixedVal )[0];
      }
    catch(e) { } // font size lists can simply default to the first entry

    var minSizeVal = 0;
    try 
      {
        var minSizePref = "font.minimum-size." + languageList.value;
        minSizeVal = pref.GetIntPref( minSizePref );
      }
    catch(e) { }
    minSizeSelect( minSizeVal );

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
        var calscreen = window.openDialog("chrome://communicator/content/pref/pref-calibrate-screen.xul", 
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
    if (items.item(0))
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

// disable font items, but not the browserUseDocumentFonts checkbox nor the resolution
// menulist
function disableAllFontElements()
  {
      var doc_ids = [ "selectLangs", "proportionalFont",
                      "sizeVar", "serif", "sans-serif","monospace",
                      "sizeMono", "minSize" ];
      for (i=0; i<doc_ids.length; i++) {
          element = document.getElementById( doc_ids[i] );
          element.disabled = true;
      }
  }

