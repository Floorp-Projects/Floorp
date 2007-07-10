/* vim:set ts=4 sts=4 sw=4 et cin: */
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
 * The Original Code is Mozilla Progress Dialog.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law       <law@netscape.com>
 *   Aaron Kaluszka <ask@swva.net>
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

/* This file implements the nsIProgressDialog interface.  See nsIProgressDialog.idl
 *
 * The implementation consists of a JavaScript "class" named nsProgressDialog,
 * comprised of:
 *   - a JS constructor function
 *   - a prototype providing all the interface methods and implementation stuff
 *
 * In addition, this file implements an nsIModule object that registers the
 * nsProgressDialog component.
 */

/* ctor
 */
function nsProgressDialog() {
    // Initialize data properties.
    this.mParent      = null;
    this.mOperation   = null;
    this.mStartTime   = ( new Date() ).getTime();
    this.observer     = null;
    this.mLastUpdate  = Number.MIN_VALUE; // To ensure first onProgress causes update.
    this.mInterval    = 750; // Default to .75 seconds.
    this.mElapsed     = 0;
    this.mLoaded      = false;
    this.fields       = new Array;
    this.strings      = new Array;
    this.mSource      = null;
    this.mTarget      = null;
    this.mTargetFile  = null;
    this.mMIMEInfo    = null;
    this.mDialog      = null;
    this.mDisplayName = null;
    this.mPaused      = false;
    this.mRequest     = null;
    this.mCompleted   = false;
    this.mMode        = "normal";
    this.mPercent     = -1;
    this.mRate        = 0;
    this.mBundle      = null;
    this.mCancelDownloadOnClose = true;
}

const nsIProgressDialog      = Components.interfaces.nsIProgressDialog;
const nsIWindowWatcher       = Components.interfaces.nsIWindowWatcher;
const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
const nsITextToSubURI        = Components.interfaces.nsITextToSubURI;
const nsIChannel             = Components.interfaces.nsIChannel;
const nsIFileURL             = Components.interfaces.nsIFileURL;
const nsIURL                 = Components.interfaces.nsIURL;
const nsILocalFile           = Components.interfaces.nsILocalFile;

nsProgressDialog.prototype = {
    // Turn this on to get debugging messages.
    debug: false,

    // Chrome-related constants.
    dialogChrome:   "chrome://global/content/nsProgressDialog.xul",
    dialogFeatures: "chrome,titlebar,minimizable=yes,dialog=no",

    // getters/setters
    get saving()            { return this.MIMEInfo == null ||
                              this.MIMEInfo.preferredAction == Components.interfaces.nsIMIMEInfo.saveToDisk; },
    get parent()            { return this.mParent; },
    set parent(newval)      { return this.mParent = newval; },
    get operation()         { return this.mOperation; },
    set operation(newval)   { return this.mOperation = newval; },
    get observer()          { return this.mObserver; },
    set observer(newval)    { return this.mObserver = newval; },
    get startTime()         { return this.mStartTime; },
    set startTime(newval)   { return this.mStartTime = newval/1000; }, // PR_Now() is in microseconds, so we convert.
    get lastUpdate()        { return this.mLastUpdate; },
    set lastUpdate(newval)  { return this.mLastUpdate = newval; },
    get interval()          { return this.mInterval; },
    set interval(newval)    { return this.mInterval = newval; },
    get elapsed()           { return this.mElapsed; },
    set elapsed(newval)     { return this.mElapsed = newval; },
    get loaded()            { return this.mLoaded; },
    set loaded(newval)      { return this.mLoaded = newval; },
    get source()            { return this.mSource; },
    set source(newval)      { return this.mSource = newval; },
    get target()            { return this.mTarget; },
    get targetFile()        { return this.mTargetFile; },
    get MIMEInfo()          { return this.mMIMEInfo; },
    set MIMEInfo(newval)    { return this.mMIMEInfo = newval; },
    get dialog()            { return this.mDialog; },
    set dialog(newval)      { return this.mDialog = newval; },
    get displayName()       { return this.mDisplayName; },
    set displayName(newval) { return this.mDisplayName = newval; },
    get paused()            { return this.mPaused; },
    get completed()         { return this.mCompleted; },
    get mode()              { return this.mMode; },
    get percent()           { return this.mPercent; },
    get rate()              { return this.mRate; },
    get kRate()             { return this.mRate / 1024; },
    get cancelDownloadOnClose() { return this.mCancelDownloadOnClose; },
    set cancelDownloadOnClose(newval) { return this.mCancelDownloadOnClose = newval; },

    set target(newval) {
        // If newval references a file on the local filesystem, then grab a
        // reference to its corresponding nsIFile.
        if (newval instanceof nsIFileURL && newval.file instanceof nsILocalFile) {
            this.mTargetFile = newval.file.QueryInterface(nsILocalFile);
        } else {
            this.mTargetFile = null;
        }

        return this.mTarget = newval;
    },

    // These setters use functions that update the dialog.
    set paused(newval)      { return this.setPaused(newval); },
    set completed(newval)   { return this.setCompleted(newval); },
    set mode(newval)        { return this.setMode(newval); },
    set percent(newval)     { return this.setPercent(newval); },
    set rate(newval)        { return this.setRate(newval); },

    // ---------- nsIProgressDialog methods ----------

    // open: Store aParentWindow and open the dialog.
    open: function( aParentWindow ) {
        // Save parent and "persist" operation.
        this.parent    = aParentWindow;

        // Open dialog using the WindowWatcher service.
        var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                   .getService( nsIWindowWatcher );
        this.dialog = ww.openWindow( this.parent,
                                     this.dialogChrome,
                                     null,
                                     this.dialogFeatures,
                                     this );
    },

    init: function( aSource, aTarget, aDisplayName, aMIMEInfo, aStartTime,
                    aTempFile, aOperation ) {
      this.source = aSource;
      this.target = aTarget;
      this.displayName = aDisplayName;
      this.MIMEInfo = aMIMEInfo;
      if ( aStartTime ) {
          this.startTime = aStartTime;
      }
      this.operation = aOperation;
    },

    // ----- nsIDownloadProgressListener/nsIWebProgressListener methods -----
    // Take advantage of javascript's function overloading feature to combine
    // similiar nsIDownloadProgressListener and nsIWebProgressListener methods
    // in one. For nsIWebProgressListener calls, the aDownload paramater will
    // always be undefined.

    // Look for STATE_STOP and update dialog to indicate completion when it happens.
    onStateChange: function( aWebProgress, aRequest, aStateFlags, aStatus, aDownload ) {
        if ( aStateFlags & nsIWebProgressListener.STATE_STOP ) {
            // if we are downloading, then just wait for the first STATE_STOP
            if ( this.targetFile != null ) {
                // we are done transferring...
                this.completed = true;
                return;
            }

            // otherwise, wait for STATE_STOP with aRequest corresponding to
            // our target.  XXX redirects might screw up this logic.
            try {
                var chan = aRequest.QueryInterface(nsIChannel);
                if (chan.URI.equals(this.target)) {
                    // we are done transferring...
                    this.completed = true;
                }
            }
            catch (e) {
            }
        }
    },

    // Handle progress notifications.
    onProgressChange: function( aWebProgress,
                                aRequest,
                                aCurSelfProgress,
                                aMaxSelfProgress,
                                aCurTotalProgress,
                                aMaxTotalProgress,
                                aDownload ) {
      return this.onProgressChange64(aWebProgress, aRequest, aCurSelfProgress,
              aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload);
    },

    onProgressChange64: function( aWebProgress,
                                  aRequest,
                                  aCurSelfProgress,
                                  aMaxSelfProgress,
                                  aCurTotalProgress,
                                  aMaxTotalProgress,
                                  aDownload ) {
        // Get current time.
        var now = ( new Date() ).getTime();

        // If interval hasn't elapsed, ignore it.
        if ( now - this.lastUpdate < this.interval ) {
            return;
        }

        // Update this time.
        this.lastUpdate = now;

        // Update elapsed time.
        this.elapsed = now - this.startTime;

        // Calculate percentage.
        if ( aMaxTotalProgress > 0) {
            this.percent = Math.floor( ( aCurTotalProgress * 100.0 ) / aMaxTotalProgress );
        } else {
            this.percent = -1;
        }

        // If dialog not loaded, then don't bother trying to update display.
        if ( !this.loaded ) {
            return;
        }

        // Update dialog's display of elapsed time.
        this.setValue( "timeElapsed", this.formatSeconds( this.elapsed / 1000 ) );

        // Now that we've set the progress and the time, update # bytes downloaded...
        // Update status (nn KB of mm KB at xx.x KB/sec)
        var status = this.getString( "progressMsg" );

        // Insert 1 is the number of kilobytes downloaded so far.
        status = this.replaceInsert( status, 1, parseInt( aCurTotalProgress/1024 + .5 ) );

        // Insert 2 is the total number of kilobytes to be downloaded (if known).
        if ( aMaxTotalProgress != "-1" ) {
            status = this.replaceInsert( status, 2, parseInt( aMaxTotalProgress/1024 + .5 ) );
        } else {
            status = this.replaceInsert( status, 2, "??" );
        }

        // Insert 3 is the download rate.
        if ( this.elapsed ) {
            // Use the download speed where available, otherwise calculate
            // rate using current progress and elapsed time.
            if ( aDownload ) {
                this.rate = aDownload.speed;
            } else {
                this.rate = ( aCurTotalProgress * 1000 ) / this.elapsed;
            }
            status = this.replaceInsert( status, 3, this.kRate.toFixed(1) );
        } else {
            // Rate not established, yet.
            status = this.replaceInsert( status, 3, "??.?" );
        }

        // All 3 inserts are taken care of, now update status msg.
        this.setValue( "status", status );

        // Update time remaining.
        if ( this.rate && ( aMaxTotalProgress > 0 ) ) {
            // Calculate how much time to download remaining at this rate.
            var rem = Math.round( ( aMaxTotalProgress - aCurTotalProgress ) / this.rate );
            this.setValue( "timeLeft", this.formatSeconds( rem ) );
        } else {
            // We don't know how much time remains.
            this.setValue( "timeLeft", this.getString( "unknownTime" ) );
        }
    },

    // Look for error notifications and display alert to user.
    onStatusChange: function( aWebProgress, aRequest, aStatus, aMessage, aDownload ) {
        // Check for error condition (only if dialog is still open).
        if ( aStatus != Components.results.NS_OK ) {
            if ( this.loaded ) {
                // Get prompt service.
                var prompter = Components.classes[ "@mozilla.org/embedcomp/prompt-service;1" ]
                                   .getService( Components.interfaces.nsIPromptService );
                // Display error alert (using text supplied by back-end).
                var title = this.getProperty( this.saving ? "savingAlertTitle" : "openingAlertTitle",
                                              [ this.fileName() ],
                                              1 );
                prompter.alert( this.dialog, title, aMessage );

                // Close the dialog.
                if ( !this.completed ) {
                    this.onCancel();
                }
            } else {
                // Error occurred prior to onload even firing.
                // We can't handle this error until we're done loading, so
                // defer the handling of this call.
                this.dialog.setTimeout( function(obj,wp,req,stat,msg){obj.onStatusChange(wp,req,stat,msg)},
                                        100, this, aWebProgress, aRequest, aStatus, aMessage );
            }
        }
    },

    // Ignore onLocationChange and onSecurityChange notifications.
    onLocationChange: function( aWebProgress, aRequest, aLocation, aDownload ) {
    },

    onSecurityChange: function( aWebProgress, aRequest, aState, aDownload ) {
    },

    // ---------- nsIObserver methods ----------
    observe: function( anObject, aTopic, aData ) {
        // Something of interest occured on the dialog.
        // Dispatch to corresponding implementation method.
        switch ( aTopic ) {
        case "onload":
            this.onLoad();
            break;
        case "oncancel":
            this.onCancel();
            break;
        case "onpause":
            this.onPause();
            break;
        case "onlaunch":
            this.onLaunch();
            break;
        case "onreveal":
            this.onReveal();
            break;
        case "onunload":
            this.onUnload();
            break;
        case "oncompleted":
            // This event comes in when setCompleted needs to be deferred because
            // the dialog isn't loaded yet.
            this.completed = true;
            break;
        default:
            break;
        }
    },

    // ---------- nsISupports methods ----------

    // This "class" supports nsIProgressDialog, nsIWebProgressListener (by virtue
    // of interface inheritance), nsIObserver, and nsISupports.
    QueryInterface: function (iid) {
        if (iid.equals(Components.interfaces.nsIProgressDialog) ||
            iid.equals(Components.interfaces.nsIDownload) ||
            iid.equals(Components.interfaces.nsITransfer) ||
            iid.equals(Components.interfaces.nsIWebProgressListener) ||
            iid.equals(Components.interfaces.nsIWebProgressListener2) ||
            iid.equals(Components.interfaces.nsIDownloadProgressListener) ||
            iid.equals(Components.interfaces.nsIObserver) ||
            iid.equals(Components.interfaces.nsIInterfaceRequestor) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    // ---------- nsIInterfaceRequestor methods ----------

    getInterface: function(iid) {
        if (iid.equals(Components.interfaces.nsIPrompt) ||
            iid.equals(Components.interfaces.nsIAuthPrompt)) {
            // use the window watcher service to get a nsIPrompt/nsIAuthPrompt impl
            var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                               .getService(Components.interfaces.nsIWindowWatcher);
            var prompt;
            if (iid.equals(Components.interfaces.nsIPrompt))
                prompt = ww.getNewPrompter(this.parent);
            else
                prompt = ww.getNewAuthPrompter(this.parent);
            return prompt;
        }
        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    // ---------- implementation methods ----------

    // Initialize the dialog.
    onLoad: function() {
        // Note that onLoad has finished.
        this.loaded = true;

        // Fill dialog.
        this.loadDialog();

        // Position dialog.
        if ( this.dialog.opener ) {
            this.dialog.moveToAlertPosition();
        } else {
            this.dialog.centerWindowOnScreen();
        }

        // Set initial focus on "keep open" box.  If that box is hidden, or, if
        // the download is already complete, then focus is on the cancel/close
        // button.  The download may be complete if it was really short and the
        // dialog took longer to open than to download the data.
        if ( !this.completed && !this.saving ) {
            this.dialogElement( "keep" ).focus();
        } else {
            this.dialogElement( "cancel" ).focus();
        }
    },

    // load dialog with initial contents
    loadDialog: function() {
        // Check whether we're saving versus opening with a helper app.
        if ( !this.saving ) {
            // Put proper label on source field.
            this.setValue( "sourceLabel", this.getString( "openingSource" ) );

            // Target is the "preferred" application.  Hide if empty.
            if ( this.MIMEInfo && 
                 this.MIMEInfo.preferredApplicationHandler &&
                 this.MIMEInfo.preferredApplicationHandler.executable ) {
                var appName = 
                  this.MIMEInfo.preferredApplicationHandler.executable.leafName;
                if ( appName == null || appName.length == 0 ) {
                    this.hide( "targetRow" );
                } else {
                    // Use the "with:" label.
                    this.setValue( "targetLabel", this.getString( "openingTarget" ) );
                    // Name of application.
                    this.setValue( "target", appName );
                }
           } else {
               this.hide( "targetRow" );
           }
        } else {
            // If target is not a local file, then hide extra dialog controls.
            if (this.targetFile != null) {
                this.setValue( "target", this.targetFile.path );
            } else {
                this.setValue( "target", this.target.spec );
                this.hide( "pauseResume" );
                this.hide( "launch" );
                this.hide( "reveal" );
            }
        }

        // Set source field.
        this.setValue( "source", this.source.spec );

        var now = ( new Date() ).getTime();

        // Initialize the elapsed time.
        if ( !this.elapsed ) {
            this.elapsed = now - this.startTime;
        }

        // Update elapsed time display.
        this.setValue( "timeElapsed", this.formatSeconds( this.elapsed / 1000 ) );
        this.setValue( "timeLeft", this.getString( "unknownTime" ) );

        // Initialize the "keep open" box.  Hide this if we're opening a helper app
        // or if we are uploading.
        if ( !this.saving || !this.targetFile ) {
            // Hide this in this case.
            this.hide( "keep" );
        } else {
            // Initialize using last-set value from prefs.
            var prefs = Components.classes[ "@mozilla.org/preferences-service;1" ]
                            .getService( Components.interfaces.nsIPrefBranch );
            if ( prefs ) {
                this.dialogElement( "keep" ).checked = prefs.getBoolPref( "browser.download.progressDnldDialog.keepAlive" );
            }
        }

        // Initialize title.
        this.setTitle();
    },

    // Cancel button stops the download (if not completed),
    // and closes the dialog.
    onCancel: function() {
        // Cancel the download, if not completed.
        if ( !this.completed ) {
            if ( this.operation ) {
                const NS_BINDING_ABORTED = 0x804b0002;
                this.operation.cancel(NS_BINDING_ABORTED);
                // XXX We're supposed to clean up files/directories.
            }
            if ( this.observer ) {
                this.observer.observe( this, "oncancel", "" );
            }
            this.paused = false;
        }
        // Test whether the dialog is already closed.
        // This will be the case if we've come through onUnload.
        if ( this.dialog ) {
            // Close the dialog.
            this.dialog.close();
        }
    },

    // onunload event means the dialog has closed.
    // We go through our onCancel logic to stop the download if still in progress.
    onUnload: function() {
        // Remember "keep dialog open" setting, if visible.
        if ( this.saving ) {
            var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService( Components.interfaces.nsIPrefBranch );
            if ( prefs ) {
                prefs.setBoolPref( "browser.download.progressDnldDialog.keepAlive", this.dialogElement( "keep" ).checked );
            }
         }
         this.dialog = null; // The dialog is history.
         if ( this.mCancelDownloadOnClose ) {
             this.onCancel();
         }
    },

    // onpause event means the user pressed the pause/resume button
    // Toggle the pause/resume state (see the function setPause(), below).i
    onPause: function() {
         this.paused = !this.paused;
    },

    // onlaunch event means the user pressed the launch button
    // Invoke the launch method of the target file.
    onLaunch: function() {
         try {
           const kDontAskAgainPref  = "browser.download.progressDnlgDialog.dontAskForLaunch";
           try {
             var pref = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
             var dontAskAgain = pref.getBoolPref(kDontAskAgainPref);
           } catch (e) {
             // we need to ask if we're unsure
             dontAskAgain = false;
           }
           if ( !dontAskAgain && this.targetFile.isExecutable() ) {
             try {
               var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                               .getService( Components.interfaces.nsIPromptService );
             } catch (ex) {
               // getService doesn't return null, it throws
               return;
             }
             var title = this.getProperty( "openingAlertTitle",
                                           [ this.fileName() ],
                                           1 );
             var msg = this.getProperty( "securityAlertMsg",
                                         [ this.fileName() ],
                                         1 );
             var dontaskmsg = this.getProperty( "dontAskAgain",
                                                [ ], 0 );
             var checkbox = {value:0};
             var okToProceed = promptService.confirmCheck(this.dialog, title, msg, dontaskmsg, checkbox);
             try {
               if (checkbox.value != dontAskAgain)
                 pref.setBoolPref(kDontAskAgainPref, checkbox.value);
             } catch (ex) {}

             if ( !okToProceed )
               return;
           }
           this.targetFile.launch();
           this.dialog.close();
         } catch ( exception ) {
             // XXX Need code here to tell user the launch failed!
             dump( "nsProgressDialog::onLaunch failed: " + exception + "\n" );
         }
    },

    // onreveal event means the user pressed the "reveal location" button
    // Invoke the reveal method of the target file.
    onReveal: function() {
         try {
             this.targetFile.reveal();
             this.dialog.close();
         } catch ( exception ) {
         }
    },

    // Get filename from the target.
    fileName: function() {
        if ( this.targetFile != null )
            return this.targetFile.leafName;
        try {
            var escapedFileName = this.target.QueryInterface(nsIURL).fileName;
            var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                         .getService(nsITextToSubURI);
            return textToSubURI.unEscapeURIForUI(this.target.originCharset, escapedFileName);
        } catch (e) {}
        return "";
    },

    // Set the dialog title.
    setTitle: function() {
        // Start with saving/opening template.
        // If percentage is not known (-1), use alternate template
        var title = this.saving
            ? ( this.percent != -1 ? this.getString( "savingTitle" ) : this.getString( "unknownSavingTitle" ) )
            : ( this.percent != -1 ? this.getString( "openingTitle" ) : this.getString( "unknownOpeningTitle" ) );


        // Use file name as insert 1.
        title = this.replaceInsert( title, 1, this.fileName() );

        // Use percentage as insert 2 (if known).
        if ( this.percent != -1 ) {
            title = this.replaceInsert( title, 2, this.percent );
        }

        // Set dialog's title property.
        if ( this.dialog ) {
            this.dialog.document.title = title;
        }
    },

    // Update the dialog to indicate specified percent complete.
    setPercent: function( percent ) {
        // Maximum percentage is 100.
        if ( percent > 100 ) {
            percent = 100;
        }
        // Test if percentage is changing.
        if ( this.percent != percent ) {
            this.mPercent = percent;

            // If dialog not opened yet, bail early.
            if ( !this.loaded ) {
                return this.mPercent;
            }

            if ( percent == -1 ) {
                // Progress meter needs to be in "undetermined" mode.
                this.mode = "undetermined";

                // Update progress meter percentage text.
                this.setValue( "progressText", "" );
            } else {
                // Progress meter needs to be in normal mode.
                this.mode = "normal";

                // Set progress meter thermometer.
                this.setValue( "progress", percent );

                // Update progress meter percentage text.
                this.setValue( "progressText", this.replaceInsert( this.getString( "percentMsg" ), 1, percent ) );
            }

            // Update title.
            this.setTitle();
        }
        return this.mPercent;
    },

    // Update download rate and dialog display.
    setRate: function( rate ) {
        this.mRate = rate;
        return this.mRate;
    },

    // Handle download completion.
    setCompleted: function() {
        // If dialog hasn't loaded yet, defer this.
        if ( !this.loaded ) {
            this.dialog.setTimeout( function(obj){obj.setCompleted()}, 100, this );
            return false;
        }
        if ( !this.mCompleted ) {
            this.mCompleted = true;

            // If the "keep dialog open" box is checked, then update dialog.
            if ( this.dialog && this.dialogElement( "keep" ).checked ) {
                // Indicate completion in status area.
                var string = this.getString( "completeMsg" );
                string = this.replaceInsert( string,
                                             1,
                                             this.formatSeconds( this.elapsed/1000 ) );
                string = this.replaceInsert( string,
                                             2,
                                             this.targetFile.fileSize >> 10 );

                this.setValue( "status", string);
                // Put progress meter at 100%.
                this.percent = 100;

                // Set time remaining to 00:00.
                this.setValue( "timeLeft", this.formatSeconds( 0 ) );

                // Change Cancel button to Close, and give it focus.
                var cancelButton = this.dialogElement( "cancel" );
                cancelButton.label = this.getString( "close" );
                cancelButton.focus();

                // Activate reveal/launch buttons if we enable them.
                var enableButtons = true;
                try {
                  var prefs = Components.classes[ "@mozilla.org/preferences-service;1" ]
                                  .getService( Components.interfaces.nsIPrefBranch );
                  enableButtons = prefs.getBoolPref( "browser.download.progressDnldDialog.enable_launch_reveal_buttons" );
                } catch ( e ) {
                }

                if ( enableButtons ) {
                    this.enable( "reveal" );
                    try {
                        if ( this.targetFile != null ) {
                            this.enable( "launch" );
                        }
                    } catch(e) {
                    }
                }

                // Disable the Pause/Resume buttons.
                this.dialogElement( "pauseResume" ).disabled = true;

                // Fix up dialog layout (which gets messed up sometimes).
                this.dialog.sizeToContent();

                // GetAttention to show the user that we're done
               this.dialog.getAttention();
            } else if ( this.dialog ) {
                this.dialog.close();
            }
        }
        return this.mCompleted;
    },

    // Set progress meter to given mode ("normal" or "undetermined").
    setMode: function( newMode ) {
        if ( this.mode != newMode ) {
            // Need to update progress meter.
            this.dialogElement( "progress" ).setAttribute( "mode", newMode );
        }
        return this.mMode = newMode;
    },

    // Set pause/resume state.
    setPaused: function( pausing ) {
        // If state changing, then update stuff.
        if ( this.paused != pausing ) {
            var string = pausing ? "resume" : "pause";
            this.dialogElement( "pauseResume" ).label = this.getString(string);

            // If we have an observer, tell it to suspend/resume
            if ( this.observer ) {
                this.observer.observe( this, pausing ? "onpause" : "onresume" , "" );
            }
        }
        return this.mPaused = pausing;
    },

    // Format number of seconds in hh:mm:ss form.
    formatSeconds: function( secs ) {
        // Round the number of seconds to remove fractions.
        secs = parseInt( secs + .5 );
        var hours = parseInt( secs/3600 );
        secs -= hours*3600;
        var mins = parseInt( secs/60 );
        secs -= mins*60;
        var result;
        if ( hours )
            result = this.getString( "longTimeFormat" );
        else
            result = this.getString( "shortTimeFormat" );

        if ( hours < 10 )
            hours = "0" + hours;
        if ( mins < 10 )
            mins = "0" + mins;
        if ( secs < 10 )
            secs = "0" + secs;

        // Insert hours, minutes, and seconds into result string.
        result = this.replaceInsert( result, 1, hours );
        result = this.replaceInsert( result, 2, mins );
        result = this.replaceInsert( result, 3, secs );

        return result;
    },

    // Get dialog element using argument as id.
    dialogElement: function( id ) {
        // Check if we've already fetched it.
        if ( !( id in this.fields ) ) {
            // No, then get it from dialog.
            try {
                this.fields[ id ] = this.dialog.document.getElementById( id );
            } catch(e) {
                this.fields[ id ] = {
                    value: "",
                    setAttribute: function(id,val) {},
                    removeAttribute: function(id) {}
                    }
            }
        }
        return this.fields[ id ];
    },

    // Set dialog element value for given dialog element.
    setValue: function( id, val ) {
        this.dialogElement( id ).value = val;
    },

    // Enable dialog element.
    enable: function( field ) {
        this.dialogElement( field ).removeAttribute( "disabled" );
    },

    // Get localizable string from properties file.
    getProperty: function( propertyId, strings, len ) {
        if ( !this.mBundle ) {
            this.mBundle = Components.classes[ "@mozilla.org/intl/stringbundle;1" ]
                             .getService( Components.interfaces.nsIStringBundleService )
                               .createBundle( "chrome://global/locale/nsProgressDialog.properties");
        }
        return len ? this.mBundle.formatStringFromName( propertyId, strings, len )
                   : this.mBundle.getStringFromName( propertyId );
    },

    // Get localizable string (from dialog <data> elements).
    getString: function ( stringId ) {
        // Check if we've fetched this string already.
        if ( !( this.strings && stringId in this.strings ) ) {
            // Presume the string is empty if we can't get it.
            this.strings[ stringId ] = "";
            // Try to get it.
            try {
                this.strings[ stringId ] = this.dialog.document.getElementById( "string."+stringId ).childNodes[0].nodeValue;
            } catch (e) {}
       }
       return this.strings[ stringId ];
    },

    // Replaces insert ("#n") with input text.
    replaceInsert: function( text, index, value ) {
        var result = text;
        var regExp = new RegExp( "#"+index );
        result = result.replace( regExp, value );
        return result;
    },

    // Hide a given dialog field.
    hide: function( field ) {
        this.dialogElement( field ).hidden = true;

        // Also hide any related separator element...
        var sep = this.dialogElement( field+"Separator" );
        if (sep)
            sep.hidden = true;
    },

    // Return input in hex, prepended with "0x" and leading zeros (to 8 digits).
    hex: function( x ) {
        return "0x" + ("0000000" + Number(x).toString(16)).slice(-8);
    },

    // Dump text (if debug is on).
    dump: function( text ) {
        if ( this.debug ) {
            dump( text );
        }
    }
}

// This Component's module implementation.  All the code below is used to get this
// component registered and accessible via XPCOM.
var module = {
    // registerSelf: Register this component.
    registerSelf: function (compMgr, fileSpec, location, type) {
        var compReg = compMgr.QueryInterface( Components.interfaces.nsIComponentRegistrar );
        compReg.registerFactoryLocation( this.cid,
                                         "Mozilla Download Progress Dialog",
                                         this.contractId,
                                         fileSpec,
                                         location,
                                         type );
    },

    // getClassObject: Return this component's factory object.
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.cid))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.factory;
    },

    /* CID for this class */
    cid: Components.ID("{F5D248FD-024C-4f30-B208-F3003B85BC92}"),

    /* Contract ID for this class */
    contractId: "@mozilla.org/progressdialog;1",

    /* factory object */
    factory: {
        // createInstance: Return a new nsProgressDialog object.
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new nsProgressDialog()).QueryInterface(iid);
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
