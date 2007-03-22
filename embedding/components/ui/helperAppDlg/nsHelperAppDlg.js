/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law    <law@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

const nsIHelperAppLauncherDialog = Components.interfaces.nsIHelperAppLauncherDialog;
const REASON_CANTHANDLE = nsIHelperAppLauncherDialog.REASON_CANTHANDLE;
const REASON_SERVERREQUEST = nsIHelperAppLauncherDialog.REASON_SERVERREQUEST;
const REASON_TYPESNIFFED = nsIHelperAppLauncherDialog.REASON_TYPESNIFFED;


/* ctor
 */
function nsHelperAppDialog() {
    // Initialize data properties.
    this.mLauncher = null;
    this.mContext  = null;
    this.mSourcePath = null;
    this.chosenApp = null;
    this.givenDefaultApp = false;
    this.strings   = new Array;
    this.elements  = new Array;
    this.updateSelf = true;
    this.mTitle    = "";
    this.mIsMac    = false;
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
        if (iid.equals(Components.interfaces.nsIHelperAppLauncherDialog) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    // ---------- nsIHelperAppLauncherDialog methods ----------

    // show: Open XUL dialog using window watcher.  Since the dialog is not
    //       modal, it needs to be a top level window and the way to open
    //       one of those is via that route).
    show: function(aLauncher, aContext, aReason)  {
         this.mLauncher = aLauncher;
         this.mContext  = aContext;
         this.mReason   = aReason;
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
         // Watch for error notifications.
         this.mIsMac = (this.mDialog.navigator.platform.indexOf( "Mac" ) != -1);
         this.progressListener.helperAppDlg = this;
         this.mLauncher.setWebProgressListener( this.progressListener );
    },

    // promptForSaveToFile:  Display file picker dialog and return selected file.
    promptForSaveToFile: function(aLauncher, aContext, aDefaultFile, aSuggestedFileExtension) {
        var result = "";

        const prefSvcContractID = "@mozilla.org/preferences-service;1";
        const prefSvcIID = Components.interfaces.nsIPrefService;
        var branch = Components.classes[prefSvcContractID].getService(prefSvcIID)
                                                          .getBranch("browser.download.");
        var dir = null;

        const nsILocalFile = Components.interfaces.nsILocalFile;
        const kDownloadDirPref = "dir";

        // Try and pull in download directory pref
        try {
            dir = branch.getComplexValue(kDownloadDirPref, nsILocalFile);
        } catch (e) { }

        var bundle = Components.classes["@mozilla.org/intl/stringbundle;1"]
                               .getService(Components.interfaces.nsIStringBundleService)
                               .createBundle("chrome://global/locale/nsHelperAppDlg.properties");

        var autoDownload = branch.getBoolPref("autoDownload");
        // If the autoDownload pref is set then just download to default download directory
        if (autoDownload && dir && dir.exists()) {
            if (aDefaultFile == "")
                aDefaultFile = bundle.GetStringFromName("noDefaultFile") + (aSuggestedFileExtension || "");
            dir.append(aDefaultFile);
            return uniqueFile(dir);
        }

        // Use file picker to show dialog.
        var nsIFilePicker = Components.interfaces.nsIFilePicker;
        var picker = Components.classes[ "@mozilla.org/filepicker;1" ]
                       .createInstance( nsIFilePicker );

        var windowTitle = bundle.GetStringFromName( "saveDialogTitle" );

        var parent = aContext
                        .QueryInterface( Components.interfaces.nsIInterfaceRequestor )
                        .getInterface( Components.interfaces.nsIDOMWindowInternal );
        picker.init( parent, windowTitle, nsIFilePicker.modeSave );
        picker.defaultString = aDefaultFile;
        if (aSuggestedFileExtension) {
            // aSuggestedFileExtension includes the period, so strip it
            picker.defaultExtension = aSuggestedFileExtension.substring(1);
        } else {
            try {
                picker.defaultExtension = this.mLauncher.MIMEInfo.primaryExtension;
            } catch (ex) {
            }
        }

        var wildCardExtension = "*";
        if ( aSuggestedFileExtension ) {
            wildCardExtension += aSuggestedFileExtension;
            picker.appendFilter( wildCardExtension, wildCardExtension );
        }

        picker.appendFilters( nsIFilePicker.filterAll );

        try {
            if (dir.exists())
                picker.displayDirectory = dir;
        } catch (e) { }

        if (picker.show() == nsIFilePicker.returnCancel || !picker.file) {
            // Null result means user cancelled.
            return null;
        }

        // If not using specified location save the user's choice of directory
        if (branch.getBoolPref("lastLocation") || autoDownload) {
            var directory = picker.file.parent.QueryInterface(nsILocalFile);
            branch.setComplexValue(kDownloadDirPref, nsILocalFile, directory);
        }

        return picker.file;
    },

    // ---------- implementation methods ----------

    // Web progress listener so we can detect errors while mLauncher is
    // streaming the data to a temporary file.
    progressListener: {
        // Implementation properties.
        helperAppDlg: null,

        // nsIWebProgressListener methods.
        // Look for error notifications and display alert to user.
        onStatusChange: function( aWebProgress, aRequest, aStatus, aMessage ) {
            if ( aStatus != Components.results.NS_OK ) {
                // Get prompt service.
                var prompter = Components.classes[ "@mozilla.org/embedcomp/prompt-service;1" ]
                                   .getService( Components.interfaces.nsIPromptService );
                // Display error alert (using text supplied by back-end).
                prompter.alert( this.dialog, this.helperAppDlg.mTitle, aMessage );

                // Close the dialog.
                this.helperAppDlg.onCancel();
                if ( this.helperAppDlg.mDialog ) {
                    this.helperAppDlg.mDialog.close();
                }
            }
        },

        // Ignore onProgressChange, onProgressChange64, onStateChange, onLocationChange, onSecurityChange, and onRefreshAttempted notifications.
        onProgressChange: function( aWebProgress,
                                    aRequest,
                                    aCurSelfProgress,
                                    aMaxSelfProgress,
                                    aCurTotalProgress,
                                    aMaxTotalProgress ) {
        },

        onProgressChange64: function( aWebProgress,
                                      aRequest,
                                      aCurSelfProgress,
                                      aMaxSelfProgress,
                                      aCurTotalProgress,
                                      aMaxTotalProgress ) {
        },

        onStateChange: function( aWebProgress, aRequest, aStateFlags, aStatus ) {
        },

        onLocationChange: function( aWebProgress, aRequest, aLocation ) {
        },

        onSecurityChange: function( aWebProgress, aRequest, state ) {
        },

        onRefreshAttempted: function( aWebProgress, aURI, aDelay, aSameURI ) {
            return true;
        }
    },

    // initDialog:  Fill various dialog fields with initial content.
    initDialog : function() {
         // Put product brand short name in prompt.
         var prompt = this.dialogElement( "prompt" );
         var modified = this.replaceInsert( prompt.firstChild.nodeValue, 1, this.getString( "brandShortName" ) );
         prompt.firstChild.nodeValue = modified;

         // Put file name in window title.
         var suggestedFileName = this.mLauncher.suggestedFileName;

         // Some URIs do not implement nsIURL, so we can't just QI.
         var url = this.mLauncher.source.clone();
         try {
           url.userPass = "";
         } catch (ex) {}
         var fname = "";
         this.mSourcePath = url.prePath;
         try {
             url = url.QueryInterface( Components.interfaces.nsIURL );
             // A url, use file name from it.
             fname = url.fileName;
             this.mSourcePath += url.directory;
         } catch (ex) {
             // A generic uri, use path.
             fname = url.path;
             this.mSourcePath += url.path;
         }

         if (suggestedFileName)
           fname = suggestedFileName;

         this.mTitle = this.replaceInsert( this.mDialog.document.title, 1, fname);
         this.mDialog.document.title = this.mTitle;

         // Put content type, filename and location into intro.
         this.initIntro(url, fname);

         var iconString = "moz-icon://" + fname + "?size=32&contentType=" + this.mLauncher.MIMEInfo.MIMEType;

         this.dialogElement("contentTypeImage").setAttribute("src", iconString);

         this.initAppAndSaveToDiskValues();

         // Initialize "always ask me" box. This should always be disabled
         // and set to true for the ambiguous type application/octet-stream.
         // Same if this dialog was forced due to a server request or type
         // sniffing
         var alwaysHandleCheckbox = this.dialogElement( "alwaysHandle" );
         if (this.mReason != REASON_CANTHANDLE ||
             this.mLauncher.MIMEInfo.MIMEType == "application/octet-stream"){
            alwaysHandleCheckbox.checked = false;
            alwaysHandleCheckbox.disabled = true;
         }
         else {
            alwaysHandleCheckbox.checked = !this.mLauncher.MIMEInfo.alwaysAskBeforeHandling;
         }

         // Position it.
         if ( this.mDialog.opener ) {
             this.mDialog.moveToAlertPosition();
         } else {
             this.mDialog.sizeToContent();
             this.mDialog.centerWindowOnScreen();
         }

         // Set initial focus
         this.dialogElement( "mode" ).focus();

         this.mDialog.document.documentElement.getButton("accept").disabled = true;
         const nsITimer = Components.interfaces.nsITimer;
         this._timer = Components.classes["@mozilla.org/timer;1"]
                                 .createInstance(nsITimer);
         this._timer.initWithCallback(this, 250, nsITimer.TYPE_ONE_SHOT);
    },

    // initIntro:
    initIntro: function(url, filename) {
        var intro = this.dialogElement( "intro" );
        var desc = this.mLauncher.MIMEInfo.description;

        var text;
        if ( this.mReason == REASON_CANTHANDLE )
          text = "intro.";
        else if (this.mReason == REASON_SERVERREQUEST )
          text = "intro.attachment.";
        else if (this.mReason == REASON_TYPESNIFFED )
          text = "intro.sniffed.";

        var modified;
        if (desc)
          modified = this.replaceInsert( this.getString( text + "label" ), 1, desc );
        else
          modified = this.getString( text + "noDesc.label" );

        modified = this.replaceInsert( modified, 2, this.mLauncher.MIMEInfo.MIMEType );
        modified = this.replaceInsert( modified, 3, filename);
        modified = this.replaceInsert( modified, 4, this.getString( "brandShortName" ));

        // if mSourcePath is a local file, then let's use the pretty path name instead of an ugly
        // url...
        var pathString = url.prePath;
        try
        {
          var fileURL = url.QueryInterface(Components.interfaces.nsIFileURL);
          if (fileURL)
          {
            var fileObject = fileURL.file;
            if (fileObject)
            {
              var parentObject = fileObject.parent;
              if (parentObject)
              {
                pathString = parentObject.path;
              }
            }
          }
        } catch(ex) {}


        intro.firstChild.nodeValue = "";
        intro.firstChild.nodeValue = modified;

        // Set the location text, which is separate from the intro text so it can be cropped
        var location = this.dialogElement( "location" );
        location.value = pathString;
        location.setAttribute( "tooltiptext", this.mSourcePath );
    },

    _timer: null,
    notify: function (aTimer) {
        try { // The user may have already canceled the dialog.
          if (!this._blurred)
            this.mDialog.document.documentElement.getButton('accept').disabled = false;
        } catch (ex) {}
        this._timer = null;
    },

    _blurred: false,
    onBlur: function(aEvent) {
        if (aEvent.target != this.mDialog.document)
          return;

        this._blurred = true;
        this.mDialog.document.documentElement.getButton("accept").disabled = true;
    },

    onFocus: function(aEvent) {
        if (aEvent.target != this.mDialog.document)
          return;

        this._blurred = false;
        if (!this._timer) {
          // Don't enable the button if the initial timer is running
          var script = "document.documentElement.getButton('accept').disabled = false";
          this.mDialog.setTimeout(script, 250);
        }
    },

    // Returns true iff opening the default application makes sense.
    openWithDefaultOK: function() {
        var result;

        // The checking is different on Windows...
        if ( this.mDialog.navigator.platform.indexOf( "Win" ) != -1 ) {
            // Windows presents some special cases.
            // We need to prevent use of "system default" when the file is
            // executable (so the user doesn't launch nasty programs downloaded
            // from the web), and, enable use of "system default" if it isn't
            // executable (because we will prompt the user for the default app
            // in that case).

            // Need to get temporary file and check for executable-ness.
            var tmpFile = this.mLauncher.targetFile;

            //  Default is Ok if the file isn't executable (and vice-versa).
            result = !tmpFile.isExecutable();
        } else {
            // On other platforms, default is Ok if there is a default app.
            // Note that nsIMIMEInfo providers need to ensure that this holds true
            // on each platform.
            result = this.mLauncher.MIMEInfo.hasDefaultHandler;
        }
        return result;
    },

    // Set "default" application description field.
    initDefaultApp: function() {
        // Use description, if we can get one.
        var desc = this.mLauncher.MIMEInfo.defaultDescription;
        if ( desc ) {
            this.dialogElement( "useSystemDefault" ).label = this.replaceInsert( this.getString( "defaultApp" ), 1, desc );
        }
    },

    // getPath:
    getPath: function(file) {
        if (this.mIsMac) {
            return file.leafName || file.path;
        }

        return file.path;
    },

    // initAppAndSaveToDiskValues:
    initAppAndSaveToDiskValues: function() {
        // Fill in helper app info, if there is any.
        this.chosenApp = this.mLauncher.MIMEInfo.preferredApplicationHandler;
        // Initialize "default application" field.
        this.initDefaultApp();

        // Fill application name textbox.
        if (this.chosenApp && this.chosenApp.path) {
            this.dialogElement( "appPath" ).value = this.getPath(this.chosenApp);
        }

        var useDefault = this.dialogElement( "useSystemDefault" );;
        if (this.mLauncher.MIMEInfo.preferredAction == this.nsIMIMEInfo.useSystemDefault &&
            this.mReason != REASON_SERVERREQUEST) {
            // Open (using system default).
            useDefault.radioGroup.selectedItem = useDefault;
        } else if (this.mLauncher.MIMEInfo.preferredAction == this.nsIMIMEInfo.useHelperApp &&
                   this.mReason != REASON_SERVERREQUEST) {
            // Open with given helper app.
            var openUsing = this.dialogElement( "openUsing" );
            openUsing.radioGroup.selectedItem = openUsing;
        } else {
            // Save to disk.
            var saveToDisk = this.dialogElement( "saveToDisk" );
            saveToDisk.radioGroup.selectedItem = saveToDisk;
        }
        // If we don't have a "default app" then disable that choice.
        if ( !this.openWithDefaultOK() ) {
            // Disable that choice.
            useDefault.hidden = true;
            // If that's the default, then switch to "save to disk."
            if ( useDefault.selected ) {
                useDefault.radioGroup.selectedItem = this.dialogElement( "saveToDisk" );
            }
        }

        // Enable/Disable choose application button and textfield
        this.toggleChoice();
    },

    // Enable pick app button if the user chooses that option.
    toggleChoice : function () {
        // See what option is selected.
        var openUsing = this.dialogElement( "openUsing" ).selected;
        this.dialogElement( "chooseApp" ).disabled = !openUsing;
        // appPath is always disabled on Mac
        this.dialogElement( "appPath" ).disabled = !openUsing || this.mIsMac;
        this.updateOKButton();
    },

    // Returns the user-selected application
    helperAppChoice: function() {
        var result = this.chosenApp;
        if (!this.mIsMac) {
            var typed  = this.dialogElement( "appPath" ).value;
            // First, see if one was chosen via the Choose... button.
            if ( result ) {
                // Verify that the user didn't type in something different later.
                if ( typed != result.path ) {
                    // Use what was typed in.
                    try {
                        result.QueryInterface( Components.interfaces.nsILocalFile ).initWithPath( typed );
                    } catch( e ) {
                        // Invalid path was typed.
                        result = null;
                    }
                }
            } else {
                // The user didn't use the Choose... button, try using what they typed in.
                result = Components.classes[ "@mozilla.org/file/local;1" ]
                    .createInstance( Components.interfaces.nsILocalFile );
                try {
                    result.initWithPath( typed );
                } catch( e ) {
                    result = null;
                }
            }
            // Remember what was chosen.
            this.chosenApp = result;
        }
        return result;
    },

    updateOKButton: function() {
        var ok = false;
        if ( this.dialogElement( "saveToDisk" ).selected )
        {
            // This is always OK.
            ok = true;
        }
        else if ( this.dialogElement( "useSystemDefault" ).selected )
        {
            // No app need be specified in this case.
            ok = true;
        }
        else
        {
            // only enable the OK button if we have a default app to use or if
            // the user chose an app....
            ok = this.chosenApp || /\S/.test( this.dialogElement( "appPath" ).value );
        }

        // Enable Ok button if ok to press.
        this.mDialog.document.documentElement.getButton( "accept" ).disabled = !ok;
    },

    // Returns true iff the user-specified helper app has been modified.
    appChanged: function() {
        return this.helperAppChoice() != this.mLauncher.MIMEInfo.preferredApplicationHandler;
    },

    updateMIMEInfo: function() {
        var needUpdate = false;
        // If current selection differs from what's in the mime info object,
        // then we need to update.
        // However, we don't want to change the action all nsIMIMEInfo objects to
        // saveToDisk if mReason is REASON_SERVERREQUEST.
        if ( this.dialogElement( "saveToDisk" ).selected &&
             this.mReason != REASON_SERVERREQUEST ) {
            needUpdate = this.mLauncher.MIMEInfo.preferredAction != this.nsIMIMEInfo.saveToDisk;
            if ( needUpdate )
                this.mLauncher.MIMEInfo.preferredAction = this.nsIMIMEInfo.saveToDisk;
        } else if ( this.dialogElement( "useSystemDefault" ).selected ) {
            needUpdate = this.mLauncher.MIMEInfo.preferredAction != this.nsIMIMEInfo.useSystemDefault;
            if ( needUpdate )
                this.mLauncher.MIMEInfo.preferredAction = this.nsIMIMEInfo.useSystemDefault;
        } else if ( this.dialogElement( "openUsing" ).selected ) {
            // For "open with", we need to check both preferred action and whether the user chose
            // a new app.
            needUpdate = this.mLauncher.MIMEInfo.preferredAction != this.nsIMIMEInfo.useHelperApp || this.appChanged();
            if ( needUpdate ) {
                this.mLauncher.MIMEInfo.preferredAction = this.nsIMIMEInfo.useHelperApp;
                // App may have changed - Update application and description
                var app = this.helperAppChoice();
                this.mLauncher.MIMEInfo.preferredApplicationHandler = app;
                this.mLauncher.MIMEInfo.applicationDescription = "";
            }
        }
        // Only care about the state of "always ask" if this dialog wasn't forced
        if ( this.mReason == REASON_CANTHANDLE )
        {
          // We will also need to update if the "always ask" flag has changed.
          needUpdate = needUpdate || this.mLauncher.MIMEInfo.alwaysAskBeforeHandling == this.dialogElement( "alwaysHandle" ).checked;

          // One last special case: If the input "always ask" flag was false, then we always
          // update.  In that case we are displaying the helper app dialog for the first
          // time for this mime type and we need to store the user's action in the mimeTypes.rdf
          // data source (whether that action has changed or not; if it didn't change, then we need
          // to store the "always ask" flag so the helper app dialog will or won't display
          // next time, per the user's selection).
          needUpdate = needUpdate || !this.mLauncher.MIMEInfo.alwaysAskBeforeHandling;

          // Make sure mime info has updated setting for the "always ask" flag.
          this.mLauncher.MIMEInfo.alwaysAskBeforeHandling = !this.dialogElement( "alwaysHandle" ).checked;
        }

        return needUpdate;
    },

    // See if the user changed things, and if so, update the
    // mimeTypes.rdf entry for this mime type.
    updateHelperAppPref: function() {
        // We update by passing this mime info into the "Edit Type" helper app
        // pref dialog.  It will update the data source and close the dialog
        // automatically.
        this.mDialog.openDialog( "chrome://communicator/content/pref/pref-applications-edit.xul",
                                 "_blank",
                                 "chrome,modal=yes,resizable=no",
                                 this );
    },

    // onOK:
    onOK: function() {
        // Verify typed app path, if necessary.
        if ( this.dialogElement( "openUsing" ).selected ) {
            var helperApp = this.helperAppChoice();
            if ( !helperApp || !helperApp.exists() ) {
                // Show alert and try again.
                var msg = this.replaceInsert( this.getString( "badApp" ), 1, this.dialogElement( "appPath" ).value );
                var svc = Components.classes[ "@mozilla.org/embedcomp/prompt-service;1" ]
                            .getService( Components.interfaces.nsIPromptService );
                svc.alert( this.mDialog, this.getString( "badApp.title" ), msg );
                // Disable the OK button.
                this.mDialog.document.documentElement.getButton( "accept" ).disabled = true;
                // Select and focus the input field if input field is not disabled
                var path = this.dialogElement( "appPath" );
                if ( !path.disabled ) {
                    path.select();
                    path.focus();
                }
                // Clear chosen application.
                this.chosenApp = null;
                // Leave dialog up.
                return false;
            }
        }

        // Remove our web progress listener (a progress dialog will be
        // taking over).
        this.mLauncher.setWebProgressListener( null );

        // saveToDisk and launchWithApplication can return errors in
        // certain circumstances (e.g. The user clicks cancel in the
        // "Save to Disk" dialog. In those cases, we don't want to
        // update the helper application preferences in the RDF file.
        try {
            var needUpdate = this.updateMIMEInfo();
            if ( this.dialogElement( "saveToDisk" ).selected )
                this.mLauncher.saveToDisk( null, false );
            else
                this.mLauncher.launchWithApplication( null, false );

            // Update user pref for this mime type (if necessary). We do not
            // store anything in the mime type preferences for the ambiguous
            // type application/octet-stream.
            if ( needUpdate &&
                 this.mLauncher.MIMEInfo.MIMEType != "application/octet-stream" )
            {
                this.updateHelperAppPref();
            }

        } catch(e) { }

        // Unhook dialog from this object.
        this.mDialog.dialog = null;

        // Close up dialog by returning true.
        return true;
    },

    // onCancel:
    onCancel: function() {
        // Remove our web progress listener.
        this.mLauncher.setWebProgressListener( null );

        // Cancel app launcher.
        try {
            const NS_BINDING_ABORTED = 0x804b0002;
            this.mLauncher.cancel(NS_BINDING_ABORTED);
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
            this.chosenApp = fp.file;
            // Update dialog.
            this.dialogElement( "appPath" ).value = this.getPath(this.chosenApp);
        }
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
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        compMgr.registerFactoryLocation( this.cid,
                                         "Mozilla Helper App Launcher Dialog",
                                         this.contractId,
                                         fileSpec,
                                         location,
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

// Since we're automatically downloading, we don't get the file picker's
// logic to check for existing files, so we need to do that here.
//
// Note - this code is identical to that in contentAreaUtils.js.
// If you are updating this code, update that code too! We can't share code
// here since this is called in a js component.
function uniqueFile(aLocalFile) {
    while (aLocalFile.exists()) {
        parts = /(-\d+)?(\.[^.]+)?$/.test(aLocalFile.leafName);
        aLocalFile.leafName = RegExp.leftContext + (RegExp.$1 - 1) + RegExp.$2;
    }
    return aLocalFile;
}
