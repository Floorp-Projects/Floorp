/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter van der Beken <peterv@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
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
 */


// Many thanks to Paul Graham, Gary Robinson and the spambayes list
// (Tim Peters et alia).  Though none of the lisp nor python code was
// used directly, many of the ideas implemented here originated with them.

// the junkmail plugin object itself
//
const NS_JUNKMAIL_CONTRACTID = 
    '@mozilla.org/messenger/filter-plugin;1?name=junkmail';
const NS_JUNKMAIL_CID =
    Components.ID('{485d5881-c4da-11d6-a7f2-e5b06fb5453c}');

const IOSERVICE_CTRID  = "@mozilla.org/network/io-service;1";
const SIS_CTRID        = "@mozilla.org/scriptableinputstream;1"
const MSGSESSION_CTRID = "@mozilla.org/messenger/services/session;1"
const PROFILEMGR_CTRID = "@mozilla.org/profile/manager;1"
const FILETPTSVC_CTRID = "@mozilla.org/network/file-transport-service;1";
const PREFSSVC_CTRID   = "@mozilla.org/preferences-service;1";
const MIMCVTSVC_CTRID  = "@mozilla.org/messenger/mimeconverter;1";

const nsIIOService       = Components.interfaces.nsIIOService;
const nsILocalFile       = Components.interfaces.nsILocalFile;
const nsIFileChannel     = Components.interfaces.nsIFileChannel;
const nsIMsgMailSession  = Components.interfaces.nsIMsgMailSession;
const nsIProfileInternal = Components.interfaces.nsIProfileInternal;
const nsIPrefService     = Components.interfaces.nsIPrefService;
//const nsIMimeConverter   = Components.interfaces.nsIMimeConverter;
const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
const nsIFileTransportService  = Components.interfaces.nsIFileTransportService;
const nsIComponentRegistrar    = Components.interfaces.nsIComponentRegistrar;
const nsIMsgFilterPlugin = Components.interfaces.nsIMsgFilterPlugin;
const nsISupports        = Components.interfaces.nsISupports;
const nsIFactory         = Components.interfaces.nsIFactory;

const NS_RDONLY = nsIFileChannel.NS_RDONLY;
const NS_RDWR = nsIFileChannel.NS_RDWR;
const NS_WRONLY = nsIFileChannel.NS_WRONLY;
const NS_CREATE_FILE = nsIFileChannel.NS_CREATE_FILE;
const NS_TRUNCATE = nsIFileChannel.NS_TRUNCATE;
const NS_APPEND = nsIFileChannel.NS_APPEND;

function do_GetService(aContractID, aInterface)
{
    return Components.classes[aContractID].getService(aInterface);
}

function do_CreateInstance(aContractID, aInterface)
{
    return Components.classes[aContractID].createInstance(aInterface);
}

const kUnknownToSpam = 0;
const kUnknownToHam = 1;
const kHamToSpam = 2;
const kSpamToHam = 3;

const kHamCount = 0;
const kSpamCount = 1;
const kSpamProb = 2;
const kTime = 3;

// Various constants for the calculations
const kUnknownProbability = 0.5;
const kMaxDiscriminators = 16;

// Constants for Graham's calculations
const kHamBias = 2;
const kMinimumProbability = 0.01;
const kMaximumProbability = 0.99;

// Constants for Robinson's calculations
const kMinDist = 0.1;
const kA = 1;
const kAoverX = 2;


var gPrefs;
var gMimeConverterService;
var gIOService;

// XXX DEBUG_START
const kDebugNone = 0;
const kDebugTokenize = 1;
const kDebugTokenCount = 2;
const kDebugCalculateProbabilities = 4;
const kDebugCalculateToken = 8;
const kDebugCalculateSpam = 16;
const kDebugClassify = 32;
const kDebugAll = kDebugTokenize | kDebugTokenCount |
                  kDebugCalculateProbabilities | kDebugCalculateToken
                  kDebugCalculateSpam | kDebugClassify;

var gDebugLevel = kDebugCalculateToken | kDebugCalculateSpam | kDebugClassify;
var gDebugStream = null;
function debugOutput(aOutput)
{
    if (!gDebugStream) {
        var profileMgr = do_GetService(PROFILEMGR_CTRID, nsIProfileInternal);
        var outFile = profileMgr.getProfileDir(profileMgr.currentProfile);
        outFile.append("bayesianspam.log");
        var fileTransportService = do_GetService(FILETPTSVC_CTRID, nsIFileTransportService);
        var trans = fileTransportService.createTransport(outFile, NS_RDWR |
                        NS_CREATE_FILE |
                        NS_APPEND, 420, false);
        gDebugStream = trans.openOutputStream(0, -1, 0);
    }
    gDebugStream.write(aOutput, aOutput.length);
    gDebugStream.flush();
}
// XXX DEBUG_END

/**
 * This is a dummy message filter which gets passed back to ApplyFilterHit()
 * I think I'm gonna propose that nsIMsgFilter get split into two interfaces
 * for cleaniness.
 */
function nsDummyFilter(aTargetFolderURI)
{
    this.mTargetFolderURI = aTargetFolderURI;
}
nsDummyFilter.prototype = 
{
    mTargetFolderURI: "",

    get action()
    {
        return Components.interfaces.nsMsgFilterAction.MoveToFolder;
    },

    get actionTargetFolderUri()
    {
        return this.mTargetFolderURI;
    }
}

/**
 * Helper class to hold the extrema as a ordered linked list.
 */
function nsExtrema()
{
}
nsExtrema.prototype =
{
    mFirst: null,
    mLength: 0,

    shouldAdd: function _shouldAddExtremum(aDistance)
    {
        return ((this.mLength < kMaxDiscriminators) ||
                (aDistance > this.mFirst.mDistance));
    },
    add: function _addExtremum(aExtremum)
    {
        var test;
        if (this.mFirst && aExtremum.mDistance > this.mFirst.mDistance) {
            var current = this.mFirst;
            while (current.mNext &&
                   aExtremum.mDistance > current.mNext.mDistance) {
                current = current.mNext;
            }
            test = current.mNext;
            while (test && aExtremum.mDistance == test.mDistance) {
                if (test.mToken == aExtremum.mToken) {
                    return;
                }
                test = test.mNext;
            }
            aExtremum.mNext = current.mNext;
            current.mNext = aExtremum;
        }
        else {
            test = this.mFirst;
            while (test && aExtremum.mDistance == test.mDistance) {
                if (test.mToken == aExtremum.mToken) {
                    return;
                }
                test = test.mNext;
            }
            aExtremum.mNext = this.mFirst;
            this.mFirst = aExtremum;
        }
        if (this.mLength == kMaxDiscriminators) {
// XXX DEBUG_START
            if (gDebugLevel & kDebugCalculateToken) {
                debugOutput("Dropping token " + this.mFirst.mToken + " from extrema\n");
            }
// XXX DEBUG_END
            this.mFirst = this.mFirst.mNext;
        }
        else {
            ++this.mLength;
        }
    }
}

/**
 * Helper to drive the tokenizer over a stream.
 */
function nsStreamListener(aComponent, aStartFunction, aTokenFunction,
                          aStopFunction)
{
    this.mTokenize = aComponent.tokenize;
    this.mStartFunction = aStartFunction;
    this.mTokenFunction = aTokenFunction;
    this.mStopFunction = aStopFunction;
}
nsStreamListener.prototype =
{
    mTokenize: null,
    mStartFunction: null,
    mTokenFunction: null,
    mStopFunction: null,
    mSis: null,

    onStartRequest: function _onStartRequest(aRequest, aData)
    {
        if (this.mStartFunction) {
            this.mStartFunction();
        }
    },
    
    onDataAvailable: function _onDataAvailable(aRequest, aContext, aStream,
                                               aOffset, aCount)
    {
        if (!this.mSis)
        {
            this.mSis = do_CreateInstance(SIS_CTRID, nsIScriptableInputStream);
            this.mSis.init(aStream);
        }
        var data = this.mSis.read(aCount);
        this.mTokenize(data, this.mTokenFunction);
    },

    onStopRequest: function _onStopRequest(aRequest, aData, aStatus)
    {
        if (this.mStopFunction) {
            this.mStopFunction();
        }
    }    
}

/**
 * The base class that holds the word stats and serializes/deserializes them.
 */
function nsBaseStatsTable()
{
}
nsBaseStatsTable.prototype =
{
    mLoadDone: false,
    mHamCount: 0,
    mSpamCount: 0,
    mHash: {},
    mBatchUpdateLevel: 0,

    ensureHashIsLoaded: function _ensureHashIsLoaded()
    {
        if (this.mLoadDone) {
            return;
        }
        this.mLoadDone = true;

        var profileMgr = do_GetService(PROFILEMGR_CTRID, nsIProfileInternal);
        var inFile = profileMgr.getProfileDir(profileMgr.currentProfile);
        inFile.append("spam.db");
        if (!inFile.exists()) {
            return;
        }

        var fileTransportService = do_GetService(FILETPTSVC_CTRID, nsIFileTransportService);
        var trans = fileTransportService.createTransport(inFile, NS_RDONLY, 'r', true);
        var input = trans.openInputStream(0, -1, 0);
        var inputStream = do_CreateInstance(SIS_CTRID, nsIScriptableInputStream);
        inputStream.init(input);

        var totals = /(\d+)\t(\d+)\n/g;
        totals.lastIndex = 0;
        var tokens = /(.+)\t(\d+)\t(\d+)(\t(.+))?\n/g;
//        var tokens = /(.+)\t(\d+)\t(\d+)\t(.+)\n/g;

        var buffer;
        do {
            buffer += inputStream.read(1024 * 16);

            if (totals.lastIndex == 0) {
                var totalValues = totals.exec(buffer);
                this.mHamCount = parseInt(totalValues[1]);
                this.mSpamCount = parseInt(totalValues[2]);
            }

            var stats;
            var lastMatch;
            tokens.lastIndex = 0;
            while ((stats = tokens.exec(buffer))) {
                var hamCount = parseInt(stats[2]);
                var spamCount = parseInt(stats[3]);
//                var date = parseInt(stats[4]);
                this.mHash[stats[1]] = [hamCount, spamCount, 0];
//                this.mHash[stats[1]] = [hamCount, spamCount, 0, date];
                lastMatch = tokens.lastIndex;
            }
            if (lastMatch < buffer.length) {
                buffer = buffer.substring(lastMatch, buffer.length);
            }
            else {
                buffer = "";
            }
        } while (inputStream.available());
        this.updateProbabilities();
    },

    writeHash: function _writeHash()
    {
        var profileMgr = do_GetService(PROFILEMGR_CTRID, nsIProfileInternal);
        var outFile = profileMgr.getProfileDir(profileMgr.currentProfile);
        outFile.append("spam.db");
        var fileTransportService = do_GetService(FILETPTSVC_CTRID, nsIFileTransportService);
        const ioFlags = NS_WRONLY | NS_CREATE_FILE | NS_TRUNCATE;
        var trans = fileTransportService.createTransport(outFile, ioFlags, 'w', true);
        var out = trans.openOutputStream(0, -1, 0);

        var totals = this.mHamCount.toString() + "\t" +
                     this.mSpamCount.toString() + "\n";
        out.write(totals, totals.length);
        for (token in this.mHash) {
            var record = this.mHash[token];
            var tokenString = token + "\t" + record[kHamCount].toString() +
                              "\t" + record[kSpamCount].toString() +
                              "\n";
//                              "\t" + record[kTime].toString() + "\n";
            out.write(tokenString, tokenString.length);
        }

        out.close();
    },

    addTokenToHash: function _addTokenToHash(aToken, aAction)
    {
        if (!(aToken in this.mHash)) {
            this.mHash[aToken] = [0, 0, null];
//            this.mHash[aToken] = [0, 0, null, Date.now()];
        }

        var value = this.mHash[aToken];

// XXX DEBUG_START
        if (gDebugLevel & kDebugTokenCount) {
            var output = "Changing " + aToken;
            switch (aAction) {
                case kSpamToHam:
                    output += " from spam to ham\n"
                    break;
                case kUnknownToHam:
                    output += " from unknown to ham\n"
                    break;
                case kHamToSpam:
                    output += " from ham to spam\n"
                    break;
                case kUnknownToSpam:
                    output += " from unknown to spam\n"
                    break;
            }
            output += "    Old counts: " + value[kHamCount] + " " + value[kSpamCount] + "\n";
        }
// XXX DEBUG_END

        switch (aAction) {
            case kSpamToHam:
                --value[kSpamCount];
            case kUnknownToHam:
                ++value[kHamCount];
                break;
            case kHamToSpam:
                --value[kHamCount];
            case kUnknownToSpam:
                ++value[kSpamCount];
                break;
        }

// XXX DEBUG_START
        if (gDebugLevel & kDebugTokenCount) {
            output += "    New counts: " + value[kHamCount] + " " + value[kSpamCount] + "\n";
            debugOutput(output);
        }
// XXX DEBUG_END
    }
}

/**
 * Class implementing Paul Graham's calculations.
 */
function nsGrahamCalculations()
{
}
nsGrahamCalculations.prototype = new nsBaseStatsTable();

nsGrahamCalculations.prototype.calculateToken =
    function _grahamCalculateToken(aToken, aExtrema, aTime)
    {
        var prob;
        if (aToken in this.mHash) {
            var record = this.mHash[aToken];
//            record[kTime] = aTime;
            if (!(prob = record[kSpamProb])) {
                prob = kUnknownProbability;
            }
        }
        else {
            prob = kUnknownProbability;
        }
        var distance = Math.abs(prob - 0.5);
        if (prob < kMinimumProbability) {
            if (!(mMinWords in aExtrema)) {
                aExtrema.mMinWords = new Array();
            }
            aExtrema.mMinWords.push({ mDistance: distance,
                                      mProb: prob,
                                      mToken: aToken });
        }
        else if (prob > kMaximumProbability) {
            if (!(mMaxWords in aExtrema)) {
                aExtrema.mMaxWords = new Array();
            }
            aExtrema.mMaxWords.push({ mDistance: distance,
                                      mProb: prob,
                                      mToken: aToken });
        }
        else if (aExtrema.shouldAdd(distance)) {
            aExtrema.add({ mDistance: distance,
                           mProb: prob,
                           mToken: aToken });
        }
    };

nsGrahamCalculations.prototype.calculate =
    function _grahamCalculate(aExtrema)
    {
        var minWordsLength = 0;
        if (typeof aExtrema.mMinWords != undefined) {
            minWordsLength = aExtrema.mMinWords.length;
        }
        var maxWordsLength = 0;
        if (typeof aExtrema.mMaxWords != undefined) {
            maxWordsLength = aExtrema.mMaxWords.length;
        }
        
        var difference = Math.abs(minWordsLength - maxWordsLength);
        if (difference > 0) {
            var dropLength = Math.min(minWordsLength, maxWordsLength);
            dropLength = Math.min(dropLength, kMaxDiscriminators);
            var replace;
/*
            XXX NEEDS REWRITING
            if (minWordsLength > maxWordsLength) {
                replace = aExtrema.slice(aExtrema.minWords, dropLength);
            }
            else {
                replace = aExtrema.slice(aExtrema.maxWords, dropLength);
            }
            var counter;
            for (counter = 0; counter < kMaxDiscriminators; ++counter) {
                if (aExtrema[counter].mDistance >= 0.5) {
                    var replaceCount = Math.min(dropLength, counter);
                    aExtrema.splice(counter - replaceCount,
                                    replaceCount,
                                    replace.slice(replaceCount));
                }
            }
*/
        }
        var product = 1;
        var invProduct = 1;
        var extremum = aExtrema.mFirst;
        while (extremum) {
            product *= extremum.mProb;
            invProduct *= (1 - extremum.mProb);
            extremum = extremum.mNext;
        }
        return product / (product + invProduct);
    };

nsGrahamCalculations.prototype.updateProbabilities =
    function _grahamUpdateProbabilities()
    {
        var hamTotal = Math.max(this.mHamCount, 1);
        var spamTotal = Math.max(this.mSpamCount, 1);

        var token;
        for (token in this.mHash) {
            var record = this.mHash[token];

            /**
             * Compute probabality that msg is spam if msg contains word.
             */
            var hamCount = Math.min(kHamBias * record[kHamCount], hamTotal);
            var hamRatio = hamCount / hamTotal;

            var spamCount = Math.min(record[kSpamCount], spamTotal);
            var spamRatio = spamCount / spamTotal;
            
            var prob = spamRatio / (hamRatio + spamRatio);
            if (prob < kMinimumProbability) {
                prob = kMinimumProbability;
            }
            else if (prob > kMaximumProbability) {
                prob = kMaximumProbability;
            }
            record[kSpamProb] = prob;

// XXX DEBUG_START
            if (gDebugLevel & kDebugCalculateProbabilities) {
                var output = "Graham probability for " + token + " (" + hamRatio + ", " + spamRatio + "): " + record[kSpamProb] + "\n";
                debugOutput(output);
            }
// XXX DEBUG_END
        }
    };

/**
 * Class implementing Robinson's calculations.
 */
function nsRobinsonCalculations()
{
}
nsRobinsonCalculations.prototype = new nsBaseStatsTable();

nsRobinsonCalculations.prototype.calculateToken =
    function _robinsonCalculateToken(aToken, aExtrema, aTime)
    {
        var prob;
        if (aToken in this.mHash) {
            var record = this.mHash[aToken];
//            record[kTime] = aTime;
            if (!(prob = record[kSpamProb])) {
                prob = kUnknownProbability;
            }
        }
        else {
            prob = kUnknownProbability;
        }
        var distance = Math.abs(prob - 0.5);
// XXX DEBUG_START
        if (gDebugLevel & kDebugCalculateToken) {
            debugOutput("Token: " + aToken + ", prob: " + prob + ", distance: " + distance + "\n");
        }
// XXX DEBUG_END
        if (distance > kMinDist && aExtrema.shouldAdd(distance)) {
// XXX DEBUG_START
            if (gDebugLevel & kDebugCalculateToken) {
                debugOutput("Token " + aToken + " added to extrema\n");
            }
// XXX DEBUG_END
            aExtrema.add({ mDistance: distance,
                           mProb: prob,
                           mToken: aToken });
        }
    };

nsRobinsonCalculations.prototype.calculate =
    function _robinsonCalculate(aExtrema)
    {
        var cluesCount = 0;
        var P = 1;
        var Q = 1;
        var extremum = aExtrema.mFirst;
        while (extremum) {
            ++cluesCount;
            P *= 1 - extremum.mProb;
            Q *= extremum.mProb;
            extremum = extremum.mNext;
        }
        var prob;
        if (cluesCount > 0) {
            P = 1 - Math.pow(P, (1 / cluesCount)); // spamminess
            Q = 1 - Math.pow(Q, (1 / cluesCount)); // non-spamminess
            prob = 0.5 + ((P - Q)/(2 * (P + Q)));

// XXX DEBUG_START
            if (gDebugLevel & kDebugCalculateSpam) {
                var output = "Robinson calculation: cluesCount=" + cluesCount + ", P=" + P + ", Q=" + Q + ", prob=" + prob + "\n";
                debugOutput(output);
            }
// XXX DEBUG_END
        }
        else {
            prob = 0.5;
        }
        return prob;
    };

nsRobinsonCalculations.prototype.updateProbabilities =
    function _robinsonUpdateProbabilities()
    {
        var hamTotal = Math.max(this.mHamCount, 1);
        var spamTotal = Math.max(this.mSpamCount, 1);

        var token;
        for (token in this.mHash) {
            var record = this.mHash[token];

            /**
             * Compute probabality that msg is spam if msg contains word.
             * This is the Graham calculation, but stripped of biases, and
             * of clamping into 0.01 thru 0.99.
             */
            var hamCount = Math.min(record[kHamCount], hamTotal);
            var hamRatio = hamCount / hamTotal;

            var spamCount = Math.min(record[kSpamCount], spamTotal);
            var spamRatio = spamCount / spamTotal;
            
            var prob = spamRatio / (hamRatio + spamRatio);

            /**
             * Now do Robinson's Bayesian adjustment.
             *
             *         a + (n * p(w))
             * f(w) = ---------------
             *          (a / x) + n
             */
            var n = hamCount + spamCount;
            record[kSpamProb] = (kA + (n * prob)) / (kAoverX + n);

// XXX DEBUG_START
            if (gDebugLevel & kDebugCalculateProbabilities) {
                var output = "Robinson probability for " + token + " (" + hamRatio + ", " + spamRatio + "): " + record[kSpamProb] + "\n";
                debugOutput(output);
            }
// XXX DEBUG_END
        }
    };

/**
 * The superclass for the table holding the stats. It's prototype will be
 * hooked up dynamically.
 */
function nsStatsTable()
{
}

function nsJunkmail()
{
    this.wrappedJSObject = this;
}
nsJunkmail.prototype =
{
    move: false,
    threshhold: 255.0,
    logStream: {},
    dummyFilterObj: {},
    wrappedJSObject: null,
    mTable: null,

    get mBatchUpdate()
    {
        return (this.mTable.mBatchUpdateLevel > 0);
    },
    set mBatchUpdate(aBatchUpdate)
    {
        if (aBatchUpdate) {
            ++this.mTable.mBatchUpdateLevel;
        }
        else {
            --this.mTable.mBatchUpdateLevel;
            if (this.mTable.mBatchUpdateLevel == 0) {
                this.mTable.writeHash();
                this.mTable.updateProbabilities();
            }
        }
    },
    
    initComponent: function _initComponent()
    {
        if (!gPrefs) {
            gPrefs = do_GetService(PREFSSVC_CTRID, nsIPrefService).
                     getBranch(null);
        }

        if (!gIOService) {
            gIOService = do_GetService(IOSERVICE_CTRID, nsIIOService);
        }

        if (gPrefs.prefHasUserValue("extensions.bayesianspam.algorithm")) {
            var algorithm = gPrefs.getCharPref("extensions.bayesianspam.algorithm");
            if (algorithm == "Graham") {
                nsStatsTable.prototype = new nsGrahamCalculations();
            }
            else {
                nsStatsTable.prototype = new nsRobinsonCalculations();
            }
        }
        else {
            nsStatsTable.prototype = new nsRobinsonCalculations();
        }
        this.mTable = new nsStatsTable();
    },

    init: function _init(aServerKey)
    {
        this.initComponent();

        //if (!gMimeConverterService) {
        //gMimeConverterService = do_GetService(MIMCVTSVC_CTRID, 
        //nsIMimeConverter);
        //}

        this.dummyFilterObj = new nsDummyFilter(
            gPrefs.getCharPref("mail.server." + aServerKey + ".type") +
            "://" + 
            gPrefs.getCharPref("mail.server." + aServerKey + ".userName") +
            "@"  + 
            gPrefs.getCharPref("mail.server." + aServerKey + ".hostname") +
            "/Junk Mail");
        
        // this.threshhold = gPrefs.getIntPref("mail.server." + aServerKey 
        // + ".junkmail.threshhold");

        // the logfile is called "junklog.txt" in the server directory
        //
        var logFile = gPrefs.getComplexValue("mail.server." 
                                             + aServerKey 
                                             + ".directory",
                                             nsILocalFile);
        logFile.append("junklog.txt");

        var fileTransportService = do_GetService(FILETPTSVC_CTRID, nsIFileTransportService);
        var trans = fileTransportService.createTransport(logFile, NS_RDWR | NS_CREATE_FILE | NS_APPEND,
                                                         420,        // -rw-r--r--
                                                         false);
        this.logStream = trans.openOutputStream(0, -1, 0);
    },

    filterMsgByHeaders: function _filterMsgByHeaders(aMsgHdr, aCount, aHeaders, aListener, aMsgWindow)
    { 
        var headersByName = {};
        var msgScore = 0.0;

        // returns all values of a header, joined with \n
        //
        function getHeaderVals(aHeaderName)
        {
            if (aHeaderName in headersByName) {
                return headersByName[aHeaderName].join("\n");
            } else if (aHeaderName == "ToCc") {
                // This logic is closely cribbed from SpamAssassin's
                // PerMsgStatus.pm:get_header(), note that it could end
                // up with a trailing comma (bug?).
                //
                // XXX need to get the raw values for this, not the decoded
                // values
                //
                // XXX probably shouldn't generate this info dynamically,
                // should add it to the table when the headers are read
                // 
                var toCc = getHeaderVals("To");
                if (toCc && toCc != '') {
                    toCc = toCc.slice(0,-1); // throw out the trailing newline
                    if ( toCc.search(/\S/ != -1) ) {
                        toCc += ", ";
                    }
                    
                    var cc = getHeaderVals("Cc"); // append the cc list
                    if (cc) {
                        toCc += cc;
                    }

                    if (toCc.length != 0) { // if we've got anything, return it
                        return toCc;
                    }
                }
                return undefined;
            }
            return undefined;
        }

        // returns all values of a header in array form
        //
        function getHeaderValsArray(aHeaderName)
        {
            if (aHeaderName in headersByName) {
                return headersByName[aHeaderName];
            }
            return undefined;
        }

        function buildHeaderTable()
        {
            // Build a table of arrays of lightly munged headers by
            // name.  Leading whitespace on the header value has been
            // eliminated,
            // and continuations replaced by single space characters.
            //
            var headerRE = /^(\S+)\:\s+/g;  // global so lastIndex is set

            for (var headerLine in aHeaders) {
                if (aHeaders[headerLine] == "") {
                    continue;   // ignore any empty lines
                }

                headerRE.lastIndex = 0;     // clear lastIndex
                var matches = headerRE.exec(aHeaders[headerLine]);
                try {
                    var headerName = matches[1];
                } catch (ex) {
                    debug("_EXCEPTION_: failed to match '" 
                          + aHeaders[headerLine] +"'\n");
                }

                if (! (headerName in headersByName)) {
                    headersByName[headerName] = new Array();
                }

                // save the value of the header, with all continuations 
                // replaced
                // XXX maybe let decodeMimeHeaderToCString do this
                //
                var headerValue = aHeaders[headerLine]
                    .substring(headerRE.lastIndex)
                    .replace(/\n\s+/g, " ");


                // try and decode the header
                //
                // XXX deal with different default charset depending on
                // what user has set?
                //
                var decodedValue = 
                    gMimeConverterService.decodeMimeHeaderToCString(
                        headerValue, null, false, true);

                // push the decoded header (if decoding was required) or the
                // headerValue itself (if not) onto the array of like-named
                // headers
                //
                headersByName[headerName].push(
                    decodedValue ? decodedValue : headerValue);
            }
        }

        buildHeaderTable();

        // write some info about this message to the log
        //
        var fromVals = getHeaderVals("From");
        if (fromVals) {
            var logMsg = "From: " + fromVals + "\n";
            this.logStream.write(logMsg, logMsg.length);
        }
        var subjVals = getHeaderVals("Subject");
        if (subjVals) {
            logMsg = "Subject: " + subjVals + "\n";
            this.logStream.write(logMsg, logMsg.length);
        }

        function addScore(aScore) {
            logMsg = "total message score: " + aScore + "\n\n";
            this.logStream.write(logMsg, logMsg.length);

            // XXX don't hardcode this
            //
            //aMsgHdr.setStringProperty("score", aScore);
            //if (aScore >= this.threshhold) {
            //    aListener.applyFilterHit(this.dummyFilterObj, aMsgWindow);
            //}
        }
        // XXX This is so wrong! We should get parts from mailnews and only handle some of them.
        //var messageURI = aMsgHdr.folder.generateMessageURI(aMsgHdr.messageKey) + "?fetchCompleteMessage=true";
        //this.calculate(messageURI, addScore);

        return;
    },

    tokenize: function _tokenize(aBuffer, aFunction)
    {
        var skipToken = /^[A-Za-z\d\/\+]+$|^--.+$)/;

        var commentToken = /<!--[^>]*-->/g;
        var commentStart = /<!--/;
        var commentEnd = /-->/;
        var insideComment = false;
    
        var fineToken = /\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}|[A-Za-z$@](?:\.(?!\.)|[\w$@'\/:-])+[A-Za-z\d$@]/g;

        var line;
        while ((line = /.+/g.exec(aBuffer))) {
            if (skipToken.test(line[0])) {
                skipToken.lastIndex = 0;
                continue;
            }

            // Remove complete comments from the line.
            var cleanLine = line[0].replace(commentToken, "");
            if (insideComment) {
                // If we're inside a comment, check if the comment ends in
                // this line. If not, skip this line.
                if (!commentEnd.test(cleanLine)) {
                    continue;
                }
                // Start scanning after the end of the comment.
                fineToken.lastIndex = commentEnd.lastIndex;
                commentEnd.lastIndex = 0;
                insideComment = false;
            }
            else {
                var index = cleanLine.search(commentStart);
                if (index != -1) {
                    // We have the start of a comment without the end. Skip
                    // anything inside the comment.
                    cleanLine.slice(0, index);
                    commentStart.lastIndex = 0;
                    insideComment = true;
                }
            }

            var fine;
            while ((fine = fineToken.exec(cleanLine))) {
                var token = fine[0].toLowerCase();
                function specialToken()
                {
                    // XXX Should use nsIParserService
                    // var id = mParserService.HTMLStringTagToId(token);
                    var skipToken;
                    switch (token) {
                        case "smtp":
                        case "esmtp":
                        {
                            skipToken = /id.*/g;
                            skipToken.lastIndex = fineToken.lastIndex;
                            if (skipToken.test(cleanLine)) {
                                fineToken.lastIndex = skipToken.lastIndex;
                                return true;
                            }
                            break;
                        }
                        case "date":
                        case "delivery-date":
                        case "message-id":
                        case "content-transfer-encoding":
                        case "mime-version":
                        {
                            skipToken = /:.*/g;
                            skipToken.lastIndex = fineToken.lastIndex;
                            if (skipToken.test(cleanLine)) {
                                fineToken.lastIndex = skipToken.lastIndex;
                                return true;
                            }
                            break;
                        }
                        case "x-mozilla-status":
                        case "x-mozilla-status2":
                        case "return-path":
                        case "content-type":
                        {
                            skipToken = /: /g;
                            skipToken.lastIndex = fineToken.lastIndex;
                            if (skipToken.test(cleanLine)) {
                                fineToken.lastIndex = skipToken.lastIndex;
                            }
                            return true;
                        }
                        case "boundary":
                        {
                            skipToken = /=.*/g;
                            skipToken.lastIndex = fineToken.lastIndex;
                            if (skipToken.test(cleanLine)) {
                                fineToken.lastIndex = skipToken.lastIndex;
                            }
                            return true;
                        }
                        case "from":
                        case "charset":
                        {
                            return true;
                        }
                    }
                    if (token in HTMLTokens) {
                        return true;
                    }
                    return false;
                }
        
                if (!specialToken()) {
                    aFunction(token);
// XXX DEBUG_START
                    if (gDebugLevel & kDebugTokenize) {
                        debugOutput("Token: " + token + "\n");
                    }
// XXX DEBUG_END
                }
// XXX DEBUG_START
                else if (gDebugLevel & kDebugTokenize) {
                    debugOutput("Ignored token: " + token + "\n");
                }
// XXX DEBUG_END
            }
        }
    },
    
    loadMessage: function _loadMessage(aUrl, aListener)
    {
        var channel = gIOService.newChannel(aUrl, null, null);
        channel.asyncOpen(aListener, null);
    },

    mark: function _mark(aMessageURL, aAction, aEndCallback)
    {
        function token(aToken)
        {
            caller.mTable.addTokenToHash(aToken, aAction);
        }
        function endData()
        {
            switch (aAction) {
                case kSpamToHam:
                    --caller.mTable.mSpamCount;
                case kUnknownToHam:
                    ++caller.mTable.mHamCount;
                    break;
                case kHamToSpam:
                    --caller.mTable.mHamCount;
                case kUnknownToSpam:
                    ++caller.mTable.mSpamCount;
                    break;
            }
            aEndCallback();
        }

        this.mTable.ensureHashIsLoaded();
        var caller = this;
        this.loadMessage(aMessageURL,
                         new nsStreamListener(this, null, token, endData));
    },

    calculate: function _calculate(aMessageURL, aEndCallback)
    {
        function token(aToken)
        {
            words.push(aToken);
            caller.mTable.calculateToken(aToken, extrema, null);
//            caller.mTable.calculateToken(aToken, extrema, time);
        }
        function endData()
        {
// XXX DEBUG_START
            if (gDebugLevel & kDebugCalculateSpam) {
                var output = "Extrema:\n";
                debugOutput(output);
                var extremum = extrema.mFirst;
                while (extremum) {
                    output = "    [" + extremum.mToken + ", " +
                             extremum.mDistance + ", " +
                             extremum.mProb + "]\n";
                    debugOutput(output);
                    extremum = extremum.mNext;
                }
            }
// XXX DEBUG_END

            score = caller.mTable.calculate(extrema);
            if (score <= 0.5) {
                for (counter = 0; counter < words.length; ++counter) {
                    caller.mTable.addTokenToHash(words[counter], kUnknownToHam);
                }
                ++caller.mTable.mHamCount;
                aEndCallback.onMessageScored(0);
            }
            else {
                for (counter = 0; counter < words.length; ++counter) {
                    caller.mTable.addTokenToHash(words[counter], kUnknownToSpam);
                }
                ++caller.mTable.mSpamCount;
                aEndCallback.onMessageScored(1);
            }
        }

        this.mTable.ensureHashIsLoaded();
        var extrema = new nsExtrema();
        var words = new Array();
        var caller = this;
//        var time = Date.now();
        this.loadMessage(aMessageURL, new nsStreamListener(this, null, token, endData));
    }
}

var nsJunkmailModule = 
{
    registerSelf: function _registerSelf(compMgr, fileSpec, location, type)
    {
        debug("*** Registering bayesian spam filter" +
              " (all right -- a JavaScript module!)\n");

        compMgr = compMgr.QueryInterface(nsIComponentRegistrar);
        compMgr.registerFactoryLocation(NS_JUNKMAIL_CID,
                                        'Bayesian spam filter',
                                        NS_JUNKMAIL_CONTRACTID,
                                        fileSpec, location,
                                        type);
    },

    getClassObject: function _getClassObject(compMgr, cid, iid)
    {
        if (!iid.equals(nsIFactory)) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }

        if (cid.equals(NS_JUNKMAIL_CID)) {
            return this.nsJunkmailFactory;
        }
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    nsJunkmailFactory:
    {
        createInstance: function _createInstance(outer, iid)
        {
            if (outer) {
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            }

            if (!iid.equals(nsIMsgFilterPlugin) &&
                !iid.equals(nsISupports)) {
                throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
            }

            return new nsJunkmail();
        }
    },

    // because of the way JS components work (the JS garbage-collector
    // keeps track of all the memory refs and won't unload until appropriate)
    // this ends up being a dummy function; it can always return true.
    //
    canUnload: function _canUnload(compMgr)
    {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec)
{
    return nsJunkmailModule;
}

const HTMLTokens =
    { "abbr": 1,
      "above": 1,
      "acronym": 1,
      "accesskey": 1,
      "align": 1,
      "all": 1,
      "alt": 1,
      "alink": 1,
      "applet": 1,
      "archive": 1,
      "axis": 1,
      "basefont": 1,
      "baseline": 1,
      "below": 1,
      "bgcolor": 1,
      "big": 1,
      "body": 1,
      "border": 1,
      "bottom": 1,
      "box": 1,
      "button": 1,
      "cellpadding": 1,
      "cellspacing": 1,
      "center": 1,
      "char": 1,
      "charoff": 1,
      "charset": 1,
      "circle": 1,
      "cite": 1,
      "colspan": 1,
      "coords": 1,
      "class": 1,
      "classid": 1,
      "clear": 1,
      "codebase": 1,
      "codetype": 1,
      "color": 1,
      "cols": 1,
      "compact": 1,
      "content": 1,
      "datetime": 1,
      "declare": 1,
      "defer": 1,
      "data": 1,
      "default": 1,
      "dfn": 1,
      "dir": 1,
      "disabled": 1,
      "face": 1,
      "font": 1,
      "frameborder": 1,
      "groups": 1,
      "head": 1,
      "headers": 1,
      "height": 1,
      "href": 1,
      "hreflang": 1,
      "hsides": 1,
      "html": 1,
      "http-equiv": 1,
      "hspace": 1,
      "iframe": 1,
      "input": 1,
      "img": 1,
      "ismap": 1,
      "justify": 1,
      "kbd": 1,
      "label": 1,
      "lang": 1,
      "language": 1,
      "left": 1,
      "lhs": 1,
      "link": 1,
      "longdesc": 1,
      "map": 1,
      "marginheight": 1,
      "marginwidth": 1,
      "media": 1,
      "meta": 1,
      "middle": 1,
      "multiple": 1,
      "name": 1,
      "nohref": 1,
      "none": 1,
      "noresize": 1,
      "noshade": 1,
      "nowrap": 1,
      "object": 1,
      "onblur": 1,
      "onchange": 1,
      "onclick": 1,
      "ondblclick": 1,
      "onfocus": 1,
      "onmousedown": 1,
      "onmouseup": 1,
      "onmouseover": 1,
      "onmousemove": 1,
      "onmouseout": 1,
      "onkeypress": 1,
      "onkeydown": 1,
      "onkeyup    ": 1,
      "onload": 1,
      "onselect": 1,
      "onunload": 1,
      "param": 1,
      "poly": 1,
      "profile": 1,
      "prompt": 1,
      "readonly": 1,
      "rect": 1,
      "rel": 1,
      "rev": 1,
      "rhs": 1,
      "right": 1,
      "rows": 1,
      "rowspan": 1,
      "rules": 1,
      "samp": 1,
      "scheme": 1,
      "scope": 1,
      "script": 1,
      "scrolling": 1,
      "select": 1,
      "selected": 1,
      "shape": 1,
      "size": 1,
      "small": 1,
      "span": 1,
      "src": 1,
      "standby": 1,
      "strike": 1,
      "strong": 1,
      "style": 1,
      "sub": 1,
      "summary": 1,
      "sup": 1,
      "tabindex": 1,
      "table": 1,
      "target": 1,
      "textarea": 1,
      "title": 1,
      "top": 1,
      "type": 1,
      "usemap": 1,
      "valign": 1,
      "value": 1,
      "valuetype": 1,
      "var": 1,
      "vlink": 1,
      "vsides": 1,
      "void": 1,
      "vspace": 1,
      "width": 1 };
