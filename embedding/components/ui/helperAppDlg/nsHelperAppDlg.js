/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *  Bill Law    <law@netscape.com>
 */

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
    this.strings   = new Array;
    this.elements  = new Array;
}

nsHelperAppDialog.prototype = {
    // Turn this on to get debugging messages.
    debug: true,

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
                                       "chrome,titlebar",
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
                           .createBundle( "chrome://global/locale/helperAppLauncher.properties");

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
                        "chrome,titlebar,minimizable",
                        aLauncher );
    },
    
    // ---------- implementation methods ----------

    // initDialog:  Fill various dialog fields with initial content.
    initDialog : function() {
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
         var title = this.replaceInsert( win.getAttribute( "title" ), 1, fname );
         win.setAttribute( "title", title );

         // Put content type and location into intro.
         this.initIntro();

         // Add special debug hook.
         if ( this.debug ) {
             var prompt = this.dialogElement( "prompt" );
             prompt.setAttribute( "onclick", "dialog.doDebug()" );
         }

         // Put explanation of default action into text box.
         this.initExplanation();

         // Set default selection (always the "default").
         this.dialogElement( "default" ).checked = true;

         // If default is not to save to disk, then make that the alternative.
         if ( this.mLauncher.MIMEInfo.preferredAction != Components.interfaces.nsIMIMEInfo.saveToDisk ) {
             this.dialogElement( "saveToDisk" ).checked = true;
         } else {
             this.dialogElement( "openUsing" ).checked = true;
         }

         // Disable selection under "different action".
         this.option();

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
    initIntro: function() {
        var intro = this.dialogElement( "intro" );
        var desc = this.mLauncher.MIMEInfo.Description;
        if ( desc != "" ) {
            // Use intro with descriptive text.
            modified = this.replaceInsert( this.getString( "intro.withDesc" ), 1, this.mLauncher.MIMEInfo.Description );
        } else {
            // Use intro without descriptive text.
            modified = this.getString( "intro.noDesc" );
        }
        modified = this.replaceInsert( modified, 2, this.mLauncher.MIMEInfo.MIMEType );
        modified = this.replaceInsert( modified, 3, this.mSourcePath );
        intro.firstChild.nodeValue = modified;
    },

    // initExplanation:
    initExplanation: function() {
        var expl = this.dialogElement( "explanation" );
        if ( this.mLauncher.MIMEInfo.preferredAction == Components.interfaces.nsIMIMEInfo.saveToDisk ) {
            expl.value = this.getString( "explanation.saveToDisk" );
        } else {
            // Default is to "open with system default."
            var appDesc = this.getString( "explanation.defaultApp" );
            if ( this.mLauncher.MIMEInfo.preferredAction != Components.interfaces.nsIMIMEInfo.useSystemDefault ) {
                // If opening using the app, we prefer to use the app description.
                appDesc = this.mLauncher.MIMEInfo.applicationDescription;
                if ( appDesc != "" ) {
                    // Use application description.
                    expl.value= this.replaceInsert( this.getString( "explanation.openUsing" ), 1, appDesc );
                } else {
                    // If no description, use the app executable name.
                    var app = this.mLauncher.MIMEInfo.preferredApplicationHandler;
                    if ( app ) {
                        // Use application path.
                        expl.value = this.replaceInsert( this.getString( "explanation.openUsing" ), 1, app.unicodePath );
                    }
                }
            }
        }
    },

    // onOK:
    onOK: function() {
        // Do what the user asked...
        if ( this.dialogElement( "default" ).checked ) {
            var nsIMIMEInfo = Components.interfaces.nsIMIMEInfo;
            // Get action from MIMEInfo...
            if ( this.mLauncher.MIMEInfo.preferredAction == nsIMIMEInfo.saveToDisk ) {
                this.mLauncher.saveToDisk( null, false );
            } else {
                this.mLauncher.launchWithApplication( this.mLauncher.MIMEInfo.preferredApplicationHandler, false );
            }
        } else {
            // Something different for this file...
            if ( this.dialogElement( "openUsing" ).checked ) {
                // If no app "chosen" then convert input string to file.
                if ( !this.chosenApp ) {
                    var app = Components.classes[ "@mozilla.org/file/local;1" ].createInstance( Components.interfaces.nsILocalFile );
                    app.initWithUnicodePath( this.dialogElement( "appName" ).value );
                    if ( !app.exists() ) {
                        // Show alert and try again.
                        var msg = this.replaceInsert( this.getString( "badApp" ), 1, app.unicodePath );
                        var dlgs = Components.classes[ "@mozilla.org/appshell/commonDialogs;1" ]
                                      .getService( Components.interfaces.nsICommonDialogs );
                        dlgs.Alert( this.mDialog, this.getString( "badApp.title" ), msg );
                        // Disable the OK button.
                        this.dialogElement( "ok" ).disabled = true;
                        // Leave dialog up.
                        return false;
                    } else {
                        // Use that app.
                        this.chosenApp = app;
                    }
                }
                this.mLauncher.launchWithApplication( this.chosenApp, false );
            } else {
                this.mLauncher.saveToDisk( null, false );
            }
        }

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

    // option:
    option: function() {
        // If "different" option is checked, then enable selections under it.
        var state = this.dialogElement( "different" ).checked;
        this.dialogElement( "saveToDisk" ).disabled = !state;
        this.dialogElement( "openUsing" ).disabled = !state;
        // Propagate state change to subfields.
        this.differentOption();
    },

    // focusAppName:
    focusAppName: function() {
        var appName = this.dialogElement( "appName" );
        appName.focus();
        appName.select();
    },

    // differentOption:
    differentOption: function() {
        // If openUsing checkbox is disabled or not selected, then disable subfields.
        var openUsing = this.dialogElement( "openUsing" );
        var state = !openUsing.disabled && openUsing.checked;
        this.dialogElement( "appName" ).disabled = !state;
        this.dialogElement( "chooseApp" ).disabled = !state;

        // If "openUsing" is enabled and checked, then focus there.
        if ( state ) {
            this.mDialog.setTimeout( "dialog.focusAppName()", 0 );
        }

        // Update Ok button.
        this.updateOKButton();
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

    // updateOKButton: Disable/enable Ok button depending on whether we've got all we need.
    updateOKButton: function() {
        var ok = false;
        if ( this.dialogElement( "default" ).checked ) {
            // This is always OK.
            ok = true;
        } else {
            if ( this.dialogElement( "saveToDisk" ).checked ) {
                // Save to disk is always Ok.
                ok = true;
            } else {
                if ( this.chosenApp || this.dialogElement( "appName" ).value != "" ) {
                    // Open using is OK if app selected.
                    ok = true;
                }
            }
        }
        // Enable Ok button if ok to press.
        this.dialogElement( "ok" ).disabled = !ok;
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
            this.userChoseApp = true;
            this.chosenApp    = fp.file;
            // Update dialog.
            this.dialogElement( "appName" ).value = this.chosenApp.unicodePath;
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
        var file_url    = Components.classes[ "@mozilla.org/network/standard-url;1" ]
                            .createInstance( Components.interfaces.nsIFileURL );
        file_url.file = file;

        // We must try creating a fresh remote DS in order to avoid accidentally
        // having GetDataSource trigger an asych load.
        var ds = Components.classes[ "@mozilla.org/rdf/datasource;1?name=xml-datasource" ].createInstance( Components.interfaces.nsIRDFDataSource );
        try {
            // Initialize it.  This will fail if the uriloader (or anybody else)
            // has already loaded/registered this data source.
            var remoteDS = ds.QueryInterface( Components.interfaces.nsIRDFRemoteDataSource );
            remoteDS.Init( file_url.spec );
            remoteDS.Refresh( true );
        } catch ( all ) {
            // OK then, presume it was already registered; get it.
            ds = rdf.GetDataSource( file_url.spec );
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
        this.initExplanation();
    },

    // updateMIMEInfo:  This is called from the pref-applications-edit dialog when the user
    //                  presses OK.  Take the updated MIMEInfo and have the helper app service
    //                  "write" it back out to the RDF datasource.
    updateMIMEInfo: function() {
        this.dump( "updateMIMEInfo called...\n" );
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
        var regExp = eval( "/#"+insertNo+"/" );
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
