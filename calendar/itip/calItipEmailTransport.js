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
 * The Original Code is Simdesk Technologies code.
 *
 * The Initial Developer of the Original Code is
 * Simdesk Technologies.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Clint Talbert <ctalbert.moz@gmail.com>
 *   Matthew Willis <lilmatt@mozilla.com>
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

/**
 * Constructor of calItipEmailTransport object
 */
function calItipEmailTransport() {
    this.wrappedJSObject = this;
    this._initEmailTransport();
}

calItipEmailTransport.prototype = {

    QueryInterface: function cietQI(aIid) {
        if (!aIid.equals(Components.interfaces.nsISupports) &&
            !aIid.equals(Components.interfaces.calIItipTransport))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mHasXpcomMail: false,
    mAccountMgrSvc: null,
    mDefaultAccount: null,
    mDefaultSmtpServer: null,

    mDefaultIdentity: null,
    get defaultIdentity() {
        return this.mDefaultIdentity.email;
    },

    mSenderAddress: null,
    get senderAddress() {
        return this.mSenderAddress;
    },
    set senderAddress(aValue) {
        return (this.mSenderAddress = aValue);
    },

    get type() {
        return "email";
    },

    /**
     * Pass the transport an itipItem and have it figure out what to do with
     * it based on the itipItem's methods.
     */
    simpleSendResponse: function cietSSR(aItem) {
        var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"].
                  getService(Components.interfaces.nsIStringBundleService);
        var sb = sbs.createBundle("chrome://lightning/locale/lightning.properties");

        var itemList = aItem.getItemList({ });

        // Get my participation status
        var myPartStat;
        var foundMyself = false;
        var attendee = itemList[0].getAttendeeById("mailto:" +
                                                   this.mDefaultIdentity.email);
        if (attendee) {
            myPartStat = attendee.participationStatus;
            foundMyself = true;
        }

        if (!foundMyself) {
            // I didn't find myself in the attendee list.
            //
            // This can happen when invitations are sent to email aliases
            // instead of mDefaultIdentity.email (ex: lilmatt vs. mwillis),
            // or if an invitation is sent to a listserv.
            //
            // We'll need to make more decisions regarding how to handle this
            // in the future. (ex: Prompt? Find myself in list?)  For now, we
            // just don't send a response.
            return;
        }

        var name = this.mDefaultIdentity.email;
        if (this.mDefaultIdentity.fullName) {
            name = this.mDefaultIdentity.fullName + " <" + name + ">";
        }

        var summary;
        if (itemList[0].getProperty("SUMMARY")) {
            summary = itemList[0].getProperty("SUMMARY");
        } else {
            summary = "";
        }
        var subj = sb.formatStringFromName("itipReplySubject", [summary], 1);

        // Generate proper body from my participation status
        var body;
        dump("\n\nthis is partstat: " + myPartStat + "\n");
        if (myPartStat == "DECLINED") {
            body = sb.formatStringFromName("itipReplyBodyDecline",
                                           [name], 1);
        } else {
            body = sb.formatStringFromName("itipReplyBodyAccept",
                                           [name], 1);
        }

        var recipients = [itemList[0].organizer];

        this.sendItems(recipients.length, recipients, subj, body, aItem);
    },

    sendItems: function cietSI(aCount, aRecipients, aSubject, aBody, aItem) {
        LOG("sendItems: Sending Email...");
        if (this.mHasXpcomMail) {
            this._sendXpcomMail(aCount, aRecipients, aSubject, aBody, aItem);
        } else {
            // Sunbird case: Call user's default mailer on system.
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }
    },

    checkForInvitations: function cietCFI(searchStart) {
        // We only need to do trigger a check for incoming invitations if we
        // are not Thunderbird.
        if (!this.mHasXpcomMail) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }
    },

    _initEmailTransport: function cietIES() {
        try {
            this.mAccountMgrSvc =
                 Components.classes["@mozilla.org/messenger/account-manager;1"].
                 getService(Components.interfaces.nsIMsgAccountManager);

            this.mDefaultAccount = this.mAccountMgrSvc.defaultAccount;
            this.mDefaultIdentity = this.mDefaultAccount.defaultIdentity;

            var smtpSvc = Components.classes["@mozilla.org/messengercompose/smtp;1"].
                          getService(Components.interfaces.nsISmtpService);
            this.mSmtpServer = smtpSvc.defaultServer;

        } catch (ex) {
            // Then we must resort to operating system specific means
            this.mHasXpcomMail = false;
        }

        // But if none of that threw then we can send an email through XPCOM.
        this.mHasXpcomMail = true;
        LOG("initEmailService: hasXpcomMail: " + this.mHasXpcomMail);
    },

    _sendXpcomMail: function cietSXM(aCount, aToList, aSubject, aBody, aItem) {
        // Save calItipItem to a temporary file.
        LOG("sendXpcomMail: Creating temp file for attachment.");
        var msgAttachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].
                            createInstance(Components.interfaces.nsIMsgAttachment);
        msgAttachment.url = this._createTempIcsFile(aItem);
        if (!msgAttachment.url) {
            LOG("sendXpcomMail: No writeable path in profile!");
            // XXX Is there an "out of disk space" error?
            throw Components.results.NS_ERROR_FAILURE;
        }

        msgAttachment.name =  "calendar.ics";
        msgAttachment.contentType = "text/calendar";
        msgAttachment.contentTypeParam = "method=" + aItem.responseMethod;
        // Destroy the attachment after sending
        msgAttachment.temporary = true;

        // compose fields for message
        var composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].
                            createInstance(Components.interfaces.nsIMsgCompFields);
        composeFields.useMultipartAlternative = true;
        composeFields.characterSet = "UTF-8";
        // TODO: xxx make this use the currently selected mail account, if
        // possible, and default to the default account if the selection
        // is unclear.
        var toList = "";
        for each (var recipient in aToList) {
            // Strip leading "mailto:" if it exists.
            var rId = recipient.id.replace(/^mailto:/i, "");

            // Prevent trailing commas.
            if (toList.length > 0) {
                toList += ",";
            }

            // Add this recipient id to the list.
            toList += rId;
        }
        composeFields.to = toList;
        composeFields.from = this.mDefaultIdentity.email;
        composeFields.replyTo = this.mDefaultIdentity.replyTo;
        composeFields.subject = aSubject;
        composeFields.body = aBody;
        composeFields.addAttachment(msgAttachment);

        // Message paramaters
        var composeParams = Components.classes["@mozilla.org/messengercompose/composeparams;1"].
                            createInstance(Components.interfaces.nsIMsgComposeParams);
        composeParams.composeFields = composeFields;
        // TODO: xxx: Make this a pref or read the default pref
        composeParams.format = Components.interfaces.nsIMsgCompFormat.PlainText;
        composeParams.type = Components.interfaces.nsIMsgCompType.New;

        var composeService = Components.classes["@mozilla.org/messengercompose;1"].
                             getService(Components.interfaces.nsIMsgComposeService);
        switch (aItem.autoResponse) {
            case (Components.interfaces.calIItipItem.USER):
                LOG("sendXpcomMail: Found USER autoResponse type.");

                // Open a compose window
                var url = "chrome://messenger/content/messengercompose/messengercompose.xul"
                composeService.OpenComposeWindowWithParams(url, composeParams);
                break;
            case (Components.interfaces.calIItipItem.AUTO):
                LOG("sendXpcomMail: Found AUTO autoResponse type.");

                var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"].
                         getService(Components.interfaces.nsIWindowWatcher);
                var win = ww.activeWindow;
                var msgCompose = Components.classes["@mozilla.org/messengercompose/compose;1"].
                                 createInstance(Components.interfaces.nsIMsgCompose);
                msgCompose.Initialize(win, composeParams);
                // Fourth param is message window, fifth param is progress.
                // TODO: xxx decide on whether or not we want progress window
                msgCompose.SendMsg(Components.interfaces.nsIMsgCompDeliverMode.Now,
                                   this.mDefaultIdentity,
                                   this.mDefaultAccount.key,
                                   null,
                                   null);
                break;
            case (Components.interfaces.calIItipItem.NONE):
                LOG("sendXpcomMail: Found NONE autoResponse type.");

                // No response
                break;
            default:
                // Unknown autoResponse type
                throw new Error("sendXpcomMail: " +
                                "Unknown autoResponse type: " +
                                aItem.autoResponse);
        }
    },

    _createTempIcsFile: function cietCTIF(aItem) {
        var path;
        var itemList = aItem.getItemList({ });
        // This is a workaround until bug 353369 is fixed.
        // Without it, we cannot roundtrip the METHOD property, so we must
        // re-add it to the ICS data as we serialize it.
        //
        // Look at the implicit assumption in the code at:
        // http://lxr.mozilla.org/seamonkey/source/calendar/base/src/calEvent.js#162
        // and it's easy to see why.
        itemList[0].setProperty("METHOD", aItem.responseMethod);
        var calText = "";
        for (var i = 0; i < itemList.length; i++) {
            calText += itemList[i].icalString;
        }

        LOG("ICS to be emailed: " + calText);

        try {
            var dirUtils = Components.classes["@mozilla.org/file/directory_service;1"].
                           createInstance(Components.interfaces.nsIProperties);
            var tempFile = dirUtils.get("TmpD", Components.interfaces.nsIFile);
            tempFile.append("itipTemp");
            tempFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0600);

            var outputStream = Components.classes["@mozilla.org/network/file-output-stream;1"].
                               createInstance(Components.interfaces.nsIFileOutputStream);
            var utf8CalText = this._convertFromUnicode("UTF-8", calText);

            // Let's write the file - constants from file-utils.js
            const MODE_WRONLY   = 0x02;
            const MODE_CREATE   = 0x08;
            const MODE_TRUNCATE = 0x20;
            outputStream.init(tempFile,
                              MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE,
                              0600, 0);
            outputStream.write(utf8CalText, utf8CalText.length);
            outputStream.close();

            var ioService = Components.classes["@mozilla.org/network/io-service;1"].
                            getService(Components.interfaces.nsIIOService);
            path = ioService.newFileURI(tempFile).spec;
        } catch (ex) {
            LOG("createTempItipFile failed! " + ex);
            path = null;
        }
        LOG("createTempItipFile path: " + path);
        return path;
    },

    _convertFromUnicode: function cietCFU(aCharset, aSrc) {
        var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"].
                               createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
        unicodeConverter.charset = aCharset;
        return unicodeConverter.ConvertFromUnicode(aSrc);
    }
};


/****
 **** module registration
 ****/

var calItipEmailTransportModule = {

    mCID: Components.ID("{d4d7b59e-c9e0-4a7a-b5e8-5958f85515f0}"),
    mContractID: "@mozilla.org/calendar/itip-transport;1?type=email",

    mUtilsLoaded: false,
    loadUtils: function itipEmailLoadUtils() {
        if (this.mUtilsLoaded)
            return;

        const jssslContractID = "@mozilla.org/moz/jssubscript-loader;1";
        const jssslIID = Components.interfaces.mozIJSSubScriptLoader;

        const iosvcContractID = "@mozilla.org/network/io-service;1";
        const iosvcIID = Components.interfaces.nsIIOService;

        var loader = Components.classes[jssslContractID].getService(jssslIID);
        var iosvc = Components.classes[iosvcContractID].getService(iosvcIID);

        // Note that unintuitively, __LOCATION__.parent == .
        // We expect to find utils in ./../js
        var appdir = __LOCATION__.parent.parent;
        appdir.append("js");
        var scriptName = "calUtils.js";

        var f = appdir.clone();
        f.append(scriptName);

        try {
            var fileurl = iosvc.newFileURI(f);
            loader.loadSubScript(fileurl.spec, this.__parent__.__parent__);
        } catch (e) {
            dump("Error while loading " + fileurl.spec + "\n");
            throw e;
        }

        this.mUtilsLoaded = true;
    },
    
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.mCID,
                                        "Calendar iTIP Email Transport",
                                        this.mContractID,
                                        fileSpec,
                                        location,
                                        type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.mCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        this.loadUtils();

        return this.mFactory;
    },

    mFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new calItipEmailTransport()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calItipEmailTransportModule;
}

function LOG(aString) {
    if (!getPrefSafe("calendar.itip.email.log", false)) {
        return;
    }
    var consoleService = Components.classes["@mozilla.org/consoleservice;1"].
                         getService(Components.interfaces.nsIConsoleService);
    consoleService.logStringMessage(aString);
    dump(aString + "\n");
}
