/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Simon Fraser (sfraser@netscape.com)
 */

/* Implementations of nsIControllerCommand for composer commands */


var gComposerCommandManager = null;

//-----------------------------------------------------------------------------------
function PrintObject(obj)
{
  dump("-----" + obj + "------\n");
  var names = "";
  for (var i in obj)
  {
    if (i == "value")
      names += i + ": " + obj.value + "\n";
    else if (i == "id")
      names += i + ": " + obj.id + "\n";
    else
      names += i + "\n";
  }
  
  dump(names + "-----------\n");
}

//-----------------------------------------------------------------------------------
function PrintNodeID(id)
{
  PrintObject(document.getElementById(id));
}

//-----------------------------------------------------------------------------------
var nsOpenCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
    fp.init(window, window.editorShell.GetString("OpenHTMLFile"), nsIFilePicker.modeOpen);
  
    // While we include "All", include filters that prefer HTML and Text files
    fp.setFilters(nsIFilePicker.filterText | nsIFilePicker.filterHTML | nsIFilePicker.filterAll);
  
    /* doesn't handle *.shtml files */
    try {
      fp.show();
      /* need to handle cancel (uncaught exception at present) */
    }
    catch (ex) {
      dump("filePicker.chooseInputFile threw an exception\n");
    }
  
    /* check for already open window and activate it... 
     * note that we have to test the native path length
     *  since fileURL.spec will be "file:///" if no filename picked (Cancel button used)
    */
    if (fp.file && fp.file.path.length > 0) {
  
      var found = FindAndSelectEditorWindowWithURL(fp.fileURL.spec);
      if (!found)
      {
        // if the existing window is untouched, just load there
        if (PageIsEmptyAndUntouched())
        {
          window.editorShell.LoadUrl(fp.fileURL.spec);
        }
        else
        {
  	      /* open new window */
  	      window.openDialog("chrome://editor/content",
  	                        "_blank",
  	                        "chrome,dialog=no,all",
  	                        fp.fileURL.spec);
        }
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsNewEditorCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
    NewEditorWindow();
  }
};

//-----------------------------------------------------------------------------------
var nsOpenRemoteCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  doCommand: function(aCommand)
  {
	  /* The last parameter is the current browser window.
	     Use 0 and the default checkbox will be to load into an editor
	     and loading into existing browser option is removed
	   */
	  window.openDialog( "chrome://navigator/content/openLocation.xul", "_blank", "chrome,modal", 0);
  }
};

//-----------------------------------------------------------------------------------
var nsPreviewCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // maybe disable if we haven't saved?
    // return (window.editorShell && !window.editorShell.documentModified);   
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
	  if (!editorShell.CheckAndSaveDocument(editorShell.GetString("BeforePreview")))
	    return;

	  var fileurl = "";
	  try {
	    fileurl = window.content.location;
	  } catch (e) {
	    return;
	  }

	  // CheckAndSave doesn't tell us if the user said "Don't Save",
	  // so make sure we have a url:
	  if (fileurl != "" && fileurl != "about:blank")
	  {
	    window.openDialog(getBrowserURL(), "EditorPreview", "chrome,all,dialog=no", fileurl);
	  }
  }
};


//-----------------------------------------------------------------------------------
var nsFindCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    window.editorShell.Find();
  }
};

//-----------------------------------------------------------------------------------
var nsFindNextCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // we can only do this if the search pattern is non-empty. Not sure how
    // to get that from here
    return (window.editorShell != null);
  },

  doCommand: function(aCommand)
  {
    window.editorShell.FindNext();
  }
};

//-----------------------------------------------------------------------------------
var nsSpellingCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell != null) && IsSpellCheckerInstalled();
  },

  doCommand: function(aCommand)
  {
    var spellChecker = window.editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
    if (spellChecker)
    {
      // dump("Check Spelling starting...\n");
      // Start the spell checker module. Return is first misspelled word
      try {
        firstMisspelledWord = spellChecker.StartSpellChecking();
      }
      catch(ex) {
        dump("*** Exception error: StartSpellChecking\n");
        return;
      }
      if( firstMisspelledWord == "")
      {
        try {
          spellChecker.CloseSpellChecking();
        }
        catch(ex) {
          dump("*** Exception error: CloseSpellChecking\n");
          return;
        }
        // No misspelled word - tell user
        window.editorShell.AlertWithTitle(window.editorShell.GetString("CheckSpelling"),
                                          window.editorShell.GetString("NoMisspelledWord")); 
      } else {
        // Set spellChecker variable on window
        window.spellChecker = spellChecker;
        try {
          window.openDialog("chrome://editor/content/EdSpellCheck.xul", "_blank",
                  "chrome,close,titlebar,modal", "", firstMisspelledWord);
        }
        catch(ex) {
          dump("*** Exception error: SpellChecker Dialog Closing\n");
          return;
        }
      }
    }
    contentWindow.focus();
  }
};

//-----------------------------------------------------------------------------------
var nsImageCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdImageProps.xul","_blank", "chrome,close,titlebar,modal");
  }
};

//-----------------------------------------------------------------------------------
var nsHLineCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    // Inserting an HLine is different in that we don't use properties dialog
    //  unless we are editing an existing line's attributes
    //  We get the last-used attributes from the prefs and insert immediately
  
    tagName = "hr";
    hLine = window.editorShell.GetSelectedElement(tagName);
  
    if (hLine) {
      dump("HLine was found -- opening dialog...!\n");
  
      // We only open the dialog for an existing HRule
      window.openDialog("chrome://editor/content/EdHLineProps.xul", "_blank", "chrome,close,titlebar,modal");
    } else {
      hLine = window.editorShell.CreateElementWithDefaults(tagName);
  
      if (hLine) {
        // We change the default attributes to those saved in the user prefs

        if (gPrefs) {
          dump(" We found the Prefs Service\n");
          var percent;
          var height;
          var shading;
          var ud = "undefined";
  
          try {
            var align = gPrefs.GetIntPref("editor.hrule.align");
            dump("Align pref: "+align+"\n");
            if (align == 0 ) {
              hLine.setAttribute("align", "left");
            } else if (align == 2) {
              hLine.setAttribute("align", "right");
            } else  {
              // Default is center
              hLine.setAttribute("align", "center");
            }
  	  
            var width = gPrefs.GetIntPref("editor.hrule.width");
            var percent = gPrefs.GetBoolPref("editor.hrule.width_percent");
            dump("Width pref: "+width+", percent:"+percent+"\n");
            if (percent)
              width = width +"%";
  
            hLine.setAttribute("width", width);
  
            var height = gPrefs.GetIntPref("editor.hrule.height");
            dump("Size pref: "+height+"\n");
            hLine.setAttribute("size", String(height));
  
            var shading = gPrefs.GetBoolPref("editor.hrule.shading");
            dump("Shading pref:"+shading+"\n");
            if (shading) {
              hLine.removeAttribute("noshade");
            } else {
              hLine.setAttribute("noshade", "");
            }
          }
          catch (ex) {
            dump("failed to get HLine prefs\n");
          }
        }
        try {
          window.editorShell.InsertElementAtSelection(hLine, true);
        } catch (e) {
          dump("Exception occured in InsertElementAtSelection\n");
        }
      }
    }
  }
};

//-----------------------------------------------------------------------------------
var nsTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    EditorInsertOrEditTable(true);
  }
};


//-----------------------------------------------------------------------------------
var nsEditTableCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    EditorInsertOrEditTable(false);
  }
};

//-----------------------------------------------------------------------------------
var nsLinkCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdLinkProps.xul","_blank", "chrome,close,titlebar,modal");
  }
};

//-----------------------------------------------------------------------------------
var nsAnchorCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdNamedAnchorProps.xul", "_blank", "chrome,close,titlebar,modal", "");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertHTMLCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdInsSrc.xul","_blank", "chrome,close,titlebar,modal,resizeable", "");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertCharsCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdInsertChars.xul", "_blank", "chrome,close,titlebar", "");
  }
};

//-----------------------------------------------------------------------------------
var nsInsertBreakCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return false;
  },
  doCommand: function(aCommand)
  {
    _EditorNotImplemented();
  }
};

//-----------------------------------------------------------------------------------
var nsInsertBreakAllCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return false;
  },
  doCommand: function(aCommand)
  {
    _EditorNotImplemented();
  }
};

//-----------------------------------------------------------------------------------
var nsListPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdListProps.xul","_blank", "chrome,close,titlebar,modal");
  }
};


//-----------------------------------------------------------------------------------
var nsPagePropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdPageProps.xul","_blank", "chrome,close,titlebar,modal,resizable", "");
  }
};


//-----------------------------------------------------------------------------------
var nsColorPropertiesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return (window.editorShell && window.editorShell.documentEditable);
  },
  doCommand: function(aCommand)
  {
    window.openDialog("chrome://editor/content/EdColorProps.xul","_blank", "chrome,close,titlebar,modal", "");
  }
};


//-----------------------------------------------------------------------------------
var nsEditHTMLCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return false;
  },
  doCommand: function(aCommand)
  {
    _EditorNotImplemented();
  }
};


//-----------------------------------------------------------------------------------
var nsPreferencesCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },
  doCommand: function(aCommand)
  {
    goPreferences('navigator.xul', 'chrome://communicator/content/pref/pref-composer.xul','editor');
  }
};


//-----------------------------------------------------------------------------------
function GetComposerController()
{
  var numControllers = window.content.controllers.getControllerCount();
  
  // count down to find a controller that supplies a nsIControllerCommandManager interface
  for (var i = numControllers; i >= 0 ; i --)
  {
    var commandManager = null;
    
    try { 
      var controller = window.content.controllers.getControllerAt(i);
      var interfaceRequestor = controller.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      if (!interfaceRequestor) continue;
    
      commandManager = interfaceRequestor.getInterface(Components.interfaces.nsIControllerCommandManager);
    } catch(ex) {
    
    }

    if (commandManager)
      return commandManager;
  }

  dump("Failed to find a controller to register commands with\n");
  return null;
}


//-----------------------------------------------------------------------------------
function SetupControllerCommands()
{
  gComposerCommandManager = GetComposerController();
  if (!gComposerCommandManager)
    return;
  
  dump("Registering commands\n");
  
  gComposerCommandManager.registerCommand("cmd_newEditor",  nsNewEditorCommand);

  gComposerCommandManager.registerCommand("cmd_open",       nsOpenCommand);
  gComposerCommandManager.registerCommand("cmd_openRemote", nsOpenRemoteCommand);
  gComposerCommandManager.registerCommand("cmd_preview",    nsPreviewCommand);

  gComposerCommandManager.registerCommand("cmd_find",       nsFindCommand);
  gComposerCommandManager.registerCommand("cmd_findNext",   nsFindNextCommand);
  gComposerCommandManager.registerCommand("cmd_spelling",   nsSpellingCommand);

  gComposerCommandManager.registerCommand("cmd_editHTML",   nsEditHTMLCommand);
  gComposerCommandManager.registerCommand("cmd_preferences", nsPreferencesCommand);


  gComposerCommandManager.registerCommand("cmd_listProperties",  nsListPropertiesCommand);
  gComposerCommandManager.registerCommand("cmd_pageProperties",  nsPagePropertiesCommand);
  gComposerCommandManager.registerCommand("cmd_colorProperties", nsColorPropertiesCommand);

  gComposerCommandManager.registerCommand("cmd_image",       nsImageCommand);
  gComposerCommandManager.registerCommand("cmd_hline",       nsHLineCommand);
  gComposerCommandManager.registerCommand("cmd_table",       nsTableCommand);   // insert or edit
  gComposerCommandManager.registerCommand("cmd_editTable",   nsEditTableCommand);   // edit
  gComposerCommandManager.registerCommand("cmd_link",        nsLinkCommand);
  gComposerCommandManager.registerCommand("cmd_anchor",      nsAnchorCommand);
  gComposerCommandManager.registerCommand("cmd_insertHTML",  nsInsertHTMLCommand);
  gComposerCommandManager.registerCommand("cmd_insertBreak", nsInsertBreakCommand);
  gComposerCommandManager.registerCommand("cmd_insertBreakAll",  nsInsertBreakAllCommand);

}

//-----------------------------------------------------------------------------------
function goUpdateComposerMenuItems(commandset)
{
  // dump("Updating commands for " + commandset.id + "\n");
  
  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID)
    {
      goUpdateCommand(commandID);
    }
  }
}

