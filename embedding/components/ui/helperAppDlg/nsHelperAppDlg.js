/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bill Law    <law@netscape.com>
 *  Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/* This file implements the nsIHelperAppLauncherDialog interface.
 *
 * The implementation consists of a JavaScript "class" named nsHelperAppDialog,
 * comprised of:
 *   - a JS constructor function
 *   - a prototype providing all the interface methods and implementation stuff
 *
 * In addition, this file implements an nsIModule object that registers the
 * nsHelperAppDialog component.
 */


/* ctor
 */
function nsHelperAppDialog() {
    // Initialize data properties.
    this.mLauncher = null;
    this.mContext  = null;
    this.mSourcePath = null;
    this.choseApp  = false;
    this.chosenApp = null;
    this.givenDefaultApp = false;
    this.strings   = new Array;
    this.elements  = new Array;
}

nsHelperAppDialog.prototype = {
    // Turn this on to get debugging messages.
    debug: false,

    nsIMIMEInfo  : Components.interfaces.nsIMIMEInfo,

    // Dump text (if debug is on).
    dump: function( text ) {
        if ( this.debug ) {
            dump( text ); 
        }
    },

    // This "class" supports nsIHelperAppLauncherDialog, and nsISupports.
    QueryInterface: function (iid) {
        if (!iid.equals(Components.interfaces.nsIHelperAppLauncherDialog) &&
            !iid.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    // ---------- nsIHelperAppLauncherDialog methods ----------

    // show: Open XUL dialog using window watcher.  Since the dialog is not
    //       modal, it needs to be a top level window and the way to open
    //       one of those is via that route).
    show: function(aLauncher, aContext)  {
         this.mLauncher = aLauncher;
         this.mContext  = aContext;
         // Display the dialog using the Window Watcher interface.
         var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                    .getService( Components.interfaces.nsIWindowWatcher );
         this.mDialog = ww.openWindow( null, // no parent
                                       "chrome://global/content/nsHelperAppDlg.xul",
                                       null,
                                       "chrome,titlebar,dialog=yes",
                                       null );
         // Hook this object to the dialog.
         this.mDialog.dialog = this;
    },

    // promptForSaveToFile:  Display file picker dialog and return selected file.
    promptForSaveToFile: function(aContext, aDefaultFile, aSuggestedFileExtension) {
        var result = "";

        // Use file picker to show dialog.
        var nsIFilePicker = Components.interfaces.nsIFilePicker;
        var picker = Components.classes[ "@mozilla.org/filepicker;1" ]
                       .createInstance( nsIFilePicker );
        var bundle = Components.classes[ "@mozilla.org/intl/stringbundle;1" ]
                       .getService( Components.interfaces.nsIStringBundleService )
                           .createBundle( "chrome://global/locale/nsHelperAppDlg.properties");

        var windowTitle = bundle.GetStringFromName( "saveDialogTitle" );
        
        var parent = aContext
                        .QueryInterface( Components.interfaces.nsIInterfaceRequestor )
                        .getInterface( Components.interfaces.nsIDOMWindowInternal );
        picker.init( parent, windowTitle, nsIFilePicker.modeSave );
        picker.defaultString = aDefaultFile;

        var wildCardExtension = "*";
        if ( aSuggestedFileExtension ) {
            wildCardExtension += aSuggestedFileExtension;
            picker.appendFilter( wildCardExtension, wildCardExtension );
        }

        picker.appendFilters( nsIFilePicker.filterAll );

        // Pull in the user's preferences and get the default download directory.
        var prefs = Components.classes[ "@mozilla.org/preferences;1" ]
                        .getService( Components.interfaces.nsIPref );
        try {
            var startDir = prefs.getFileXPref( "browser.download.dir" );
            if ( startDir.exists() ) {
                picker.displayDirectory = startDir;
            }
        } catch( exception ) {
        }

        var dlgResult = picker.show();

        if ( dlgResult == nsIFilePicker.returnCancel ) {
            throw Components.results.NS_ERROR_FAILURE;
        }


        // be sure to save the directory the user chose as the new browser.download.dir
        result = picker.file;

        if ( result ) {
            var newDir = result.parent;
            prefs.setFileXPref( "browser.download.dir", newDir );
        }
        return result;
    },
    
    // showProgressDialog:  For now, use old dialog.  At some point, the caller should be
    //                      converted to use the new generic progress dialog (when it's
    //                      finished).
    showProgressDialog: function(aLauncher, aContext) {
         // Display the dialog using the Window Watcher interface.
         var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                    .getService( Components.interfaces.nsIWindowWatcher );
         ww.openWindow( null, // no parent
                        "chrome://global/content/helperAppDldProgress.xul",
                        null,
                        "chrome,titlebar,minimizable,dialog=yes",
                        aLauncher );
    },
    
    // ---------- implementation methods ----------

    // initDialog:  Fill various dialog fields with initial content.
    initDialog : function() {
         // Check if file is executable (in which case, we will go straight to
         // "save to disk").
         var ignore1 = new Object;
         var ignore2 = new Object;
         var tmpFile = this.mLauncher.getDownloadInfo( ignore1, ignore2 );
         if ( tmpFile.isExecutable() ) {
             this.mLauncher.saveToDisk( null, false );
             this.mDialog.close();
             return;
         }

         // Put product brand short name in prompt.
         var prompt = this.dialogElement( "prompt" );
         var modified = this.replaceInsert( prompt.firstChild.nodeValue, 1, this.getString( "brandShortName" ) );
         prompt.firstChild.nodeValue = modified;

         // Put file name in window title.
         var win   = this.dialogElement( "nsHelperAppDlg" );
         var url   = this.mLauncher.source.QueryInterface( Components.interfaces.nsIURL );
         var fname = "";
         this.mSourcePath = url.prePath;
         if ( url ) {
             // A url, use file name from it.
             fname = url.fileName;
             this.mSourcePath += url.directory;
         } else {
             // A generic uri, use path.
             fname = this.mLauncher.source.path;
             this.mSourcePath += url.path;
         }

         var title = this.replaceInsert( win.getAttribute( "title" ), 1, fname);
         win.setAttribute( "title", title );

         // Put content type and location into intro.
         this.initIntro(url);

         var iconString = "moz-icon://" + fname + "?size=32&contentType=" + this.mLauncher.MIMEInfo.MIMEType;

         this.dialogElement("contentTypeImage").setAttribute("src", iconString);

         this.initAppAndSaveToDiskValues();

         // always make sure the window starts off with this checked....
         this.dialogElement( "alwaysAskMe" ).checked = true;

         // Add special debug hook.
         if ( this.debug ) {
             var prompt = this.dialogElement( "prompt" );
             prompt.setAttribute( "onclick", "dialog.doDebug()" );
         }

         // Set up dialog button callbacks.
         var object = this; // "this.onOK()" doesn't work!
         this.mDialog.doSetOKCancel( function () { return object.onOK(); },
                                     function () { return object.onCancel(); } );

         // Position it.
         if ( this.mDialog.opener ) {
             this.mDialog.moveToAlertPosition();
         } else {
             this.mDialog.centerWindowOnScreen();
         }
    },

    // initIntro:
    initIntro: function(url) {
        var intro = this.dialogElement( "intro" );
        var desc = this.mLauncher.MIMEInfo.Description;
        var modified;
        if ( desc != "" ) 
        {
          // Use intro with descriptive text.
          modified = this.replaceInsert( this.getString( "intro.withDesc" ), 1, this.mLauncher.MIMEInfo.Description );
        } 
        else 
        {
          // Use intro without descriptive text.
          modified = this.getString( "intro.noDesc" );
        }

        modified = this.replaceInsert( modified, 2, this.mLauncher.MIMEInfo.MIMEType );

        // if mSourcePath is a local file, then let's use the pretty path name instead of an ugly
        // url...
        var pathString = this.mSourcePath;
        try 
        {
          var fileURL = url.QueryInterface( Components.interfaces.nsIFileURL);
          if (fileURL)
          {
             var fileObject = fileURL.file;
             if (fileObject)
             {
               var parentObject = fileObject.parent;
               if (parentObject)
               {
                 pathString = parentObject.unicodePath;
               }
             }
          }
        } catch(ex) {}


        intro.firstChild.nodeValue = "";
        intro.firstChild.nodeValue = modified;

        // Set the location text, which is separate from the intro text so it can be cropped
        var location = this.dialogElement( "location" );
        location.value = pathString;
    },

    // initAppAndSaveToDiskValues:
    initAppAndSaveToDiskValues: function() {

        // Pre-select the choice the user made last time.
        this.chosenApp = this.mLauncher.MIMEInfo.preferredApplicationHandler;
        var applicationDescription = this.mLauncher.MIMEInfo.applicationDescription;

        if (applicationDescription != "")
        {
          this.updateApplicationName(applicationDescription); 
          this.givenDefaultApp = true;
        }
        else if (this.chosenApp && this.chosenApp.unicodePath)
        {
          // If a user-chosen application, show its path.
          this.updateApplicationName(this.chosenApp.unicodePath);
          this.choseApp = true;
        }
        else
         this.updateApplicationName(this.getString("noApplicationSpecified"));

        if ( (applicationDescription || this.choseApp) && this.mLauncher.MIMEInfo.preferredAction != this.nsIMIMEInfo.saveToDisk ) 
        {
          this.dialogElement( "openUsing" ).checked = true;
          this.dialogElement( "saveToDisk" ).checked = false;         
        }
        else 
        {
          // Save to disk.
          this.dialogElement( "saveToDisk" ).checked = true;
          this.dialogElement( "openUsing" ).checked = false;
          // Disable choose app button.
          this.dialogElement( "chooseApp" ).setAttribute( "disabled", "true" );
        }
    },

    updateApplicationName: function(newValue)
    {
      var applicationText = this.getString( "openUsingString" );
      applicationText = this.replaceInsert( applicationText, 1, newValue );
      var expl = this.dialogElement( "openUsing" );
      expl.label = applicationText;
    },

    // Enable pick app button if the user chooses that option.
    toggleChoice : function () {
        // See what option is checked.
        if ( this.dialogElement( "openUsing" ).checked ) {
            // We can enable the pick app button.
            this.dialogElement( "chooseApp" ).removeAttribute( "disabled" );
        } else {
            // We can disable the pick app button.
            this.dialogElement( "chooseApp" ).setAttribute( "disabled", "true" );
        }

       this.updateOKButton();
    },

    processAlwaysAskState : function () 
    {
      // if the user deselected the always ask checkbox, then store that on the mime object for future use...
      if (!this.dialogElement( "alwaysAskMe" ).checked)
      {
        // we first need to rest the user action if the user selected save to disk instead of open...
        // reset the preferred action in this case...we need to do this b4 setting the always ask before handling state

        if (!this.dialogElement( "openUsing" ).checked)
        this.mLauncher.MIMEInfo.preferredAction = this.nsIMIMEInfo.saveToDisk;
         

        this.mLauncher.MIMEInfo.alwaysAskBeforeHandling = false;
      }
    },
    updateOKButton: function() {
        var ok = false;
        if ( this.dialogElement( "saveToDisk" ).checked ) 
        {
            // This is always OK.
            ok = true;
        } 
        else 
        {
          // only enable the OK button if we have a default app to use or if 
          // the user chose an app....
          if ((this.choseApp && this.chosenApp.unicodePath) || this.givenDefaultApp)
            ok = true;
        }
        
        // Enable Ok button if ok to press.
        this.dialogElement( "ok" ).disabled = !ok;
    },
 
    // onOK:
    onOK: function() {

      this.processAlwaysAskState(); 

      if ( this.dialogElement( "openUsing" ).checked ) 
      {
         // If no app "chosen" then convert input string to file.
         if (this.chosenApp)
           this.mLauncher.launchWithApplication( this.chosenApp, false );
          else 
           this.mLauncher.launchWithApplication( null, false );
      }
      else
        this.mLauncher.saveToDisk( null, false );
        
      // Unhook dialog from this object.
      this.mDialog.dialog = null;

      // Close up dialog by returning true.
      return true;
     //this.mDialog.close();
    },

    // onCancel:
    onCancel: function() {
        // Cancel app launcher.
        try {
            this.mLauncher.Cancel();
        } catch( exception ) {
        }
        
        // Unhook dialog from this object.
        this.mDialog.dialog = null;

        // Close up dialog by returning true.
        return true;
    },

    // dialogElement:  Try cache; obtain from document if not there.
    dialogElement: function( id ) {
         // Check if we've already fetched it.
         if ( !( id in this.elements ) ) {
             // No, then get it from dialog.
             this.elements[ id ] = this.mDialog.document.getElementById( id );
         }
         return this.elements[ id ];
    },

    // chooseApp:  Open file picker and prompt user for application.
    chooseApp: function() {
        var nsIFilePicker = Components.interfaces.nsIFilePicker;
        var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance( nsIFilePicker );
        fp.init( this.mDialog,
                 this.getString( "chooseAppFilePickerTitle" ),
                 nsIFilePicker.modeOpen );

        // XXX - We want to say nsIFilePicker.filterExecutable or something
        fp.appendFilters( nsIFilePicker.filterAll );
        
        if ( fp.show() == nsIFilePicker.returnOK && fp.file ) {
            // Remember the file they chose to run.
            this.choseApp = true;
            this.chosenApp    = fp.file;
            // Update dialog.

            this.updateApplicationName(this.chosenApp.unicodePath);
        }
    },

    // setDefault:  Open "edit MIMEInfo" dialog (borrowed from prefs).
    setDefault: function() {
        // Get RDF service.
        var rdf = Components.classes[ "@mozilla.org/rdf/rdf-service;1" ]
                    .getService( Components.interfaces.nsIRDFService );
        // Now ask if it knows about this mime type.
        var exists = false;
        var fileLocator = Components.classes[ "@mozilla.org/file/directory_service;1" ]
                            .getService( Components.interfaces.nsIProperties );
        var file        = fileLocator.get( "UMimTyp", Components.interfaces.nsIFile );
        
        // We must try creating a fresh remote DS in order to avoid accidentally
        // having GetDataSource trigger an asych load.
        var ds = Components.classes[ "@mozilla.org/rdf/datasource;1?name=xml-datasource" ].createInstance( Components.interfaces.nsIRDFDataSource );
        try {
            // Initialize it.  This will fail if the uriloader (or anybody else)
            // has already loaded/registered this data source.
            var remoteDS = ds.QueryInterface( Components.interfaces.nsIRDFRemoteDataSource );
            remoteDS.Init( file.URL );
            remoteDS.Refresh( true );
        } catch ( all ) {
            // OK then, presume it was already registered; get it.
            ds = rdf.GetDataSource( file.URL );
        }

        // Now check if this mimetype is really in there;
        // This is done by seeing if there's a "value" arc from the mimetype resource
        // to the mimetype literal string.
        var mimeRes       = rdf.GetResource( "urn:mimetype:" + this.mLauncher.MIMEInfo.MIMEType );
        var valueProperty = rdf.GetResource( "http://home.netscape.com/NC-rdf#value" );
        var mimeLiteral   = rdf.GetLiteral( this.mLauncher.MIMEInfo.MIMEType );
        exists =  ds.HasAssertion( mimeRes, valueProperty, mimeLiteral, true );

        var dlgUrl;
        if ( exists ) {
            // Open "edit mime type" dialog.
            dlgUrl = "chrome://communicator/content/pref/pref-applications-edit.xul";
        } else {
            // Open "add mime type" dialog.
            dlgUrl = "chrome://communicator/content/pref/pref-applications-new.xul";
        }

        // Open whichever dialog is appropriate, passing this dialog object as argument.
        this.mDialog.openDialog( dlgUrl,
                                 "_blank",
                                 "chrome,modal=yes,resizable=no",
                                 this );

        // Refresh dialog with updated info about the default action.
        this.initIntro();
        this.initAppAndSaveToDiskValues();
    },

    // updateMIMEInfo:  This is called from the pref-applications-edit dialog when the user
    //                  presses OK.  Take the updated MIMEInfo and have the helper app service
    //                  "write" it back out to the RDF datasource.
    updateMIMEInfo: function() {
        this.dumpObjectProperties( "\tMIMEInfo", this.mLauncher.MIMEInfo );
    },

    // dumpInfo:
    doDebug: function() {
        const nsIProgressDialog = Components.interfaces.nsIProgressDialog;
        // Open new progress dialog.
        var progress = Components.classes[ "@mozilla.org/progressdialog;1" ]
                         .createInstance( nsIProgressDialog );
        // Show it.
        progress.open( this.mDialog );
    },

    // dumpObj:
    dumpObj: function( spec ) {
         var val = "<undefined>";
         try {
             val = eval( "this."+spec ).toString();
         } catch( exception ) {
         }
         this.dump( spec + "=" + val + "\n" );
    },

    // dumpObjectProperties
    dumpObjectProperties: function( desc, obj ) {
         for( prop in obj ) {
             this.dump( desc + "." + prop + "=" );
             var val = "<undefined>";
             try {
                 val = obj[ prop ];
             } catch ( exception ) {
             }
             this.dump( val + "\n" );
         }
    },

    // getString: Fetch data string from dialog content (and cache it).
    getString: function( id ) {
        // Check if we've fetched this string already.
        if ( !( id in this.strings ) ) {
            // Try to get it.
            var elem = this.mDialog.document.getElementById( id );
            if ( elem
                 &&
                 elem.firstChild
                 &&
                 elem.firstChild.nodeValue ) {
                this.strings[ id ] = elem.firstChild.nodeValue;
            } else {
                // If unable to fetch string, use an empty string.
                this.strings[ id ] = "";
            }
        }
        return this.strings[ id ];
    },

    // replaceInsert: Replace given insert with replacement text and return the result.
    replaceInsert: function( text, insertNo, replacementText ) {
        var result = text;
        var regExp = new RegExp("#"+insertNo);
        result = result.replace( regExp, replacementText );
        return result;
    }
}

// This Component's module implementation.  All the code below is used to get this
// component registered and accessible via XPCOM.
var module = {
    firstTime: true,

    // registerSelf: Register this component.
    registerSelf: function (compMgr, fileSpec, location, type) {
        if (this.firstTime) {
            this.firstTime = false;
            throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
        }
        compMgr.registerComponentWithType( this.cid,
                                           "Mozilla Helper App Launcher Dialog",
                                           this.contractId,
                                           fileSpec,
                                           location,
                                           true,
                                           true,
                                           type );
    },

    // getClassObject: Return this component's factory object.
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.cid)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        if (!iid.equals(Components.interfaces.nsIFactory)) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }

        return this.factory;
    },

    /* CID for this class */
    cid: Components.ID("{F68578EB-6EC2-4169-AE19-8C6243F0ABE1}"),

    /* Contract ID for this class */
    contractId: "@mozilla.org/helperapplauncherdialog;1",

    /* factory object */
    factory: {
        // createInstance: Return a new nsProgressDialog object.
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new nsHelperAppDialog()).QueryInterface(iid);
        }
    },

    // canUnload: n/a (returns true)
    canUnload: function(compMgr) {
        return true;
    }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
    return module;
}
