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
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
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

var view = 
{
    configUrl: null,
    testArray: null,
    mCurrent: null,

    browseForConfig: function()
    {
        enablePrivilege('UniversalXPConnect');
        var fp = Components.classes["@mozilla.org/filepicker;1"].
        createInstance(nsIFilePicker);
        fp.init(window,'XSLTMark Description File',nsIFilePicker.modeOpen);
        fp.appendFilter('*.conf', '*.conf');
        fp.appendFilters(nsIFilePicker.filterAll);
        var res = fp.show();

        if (res == nsIFilePicker.returnOK) {
            this.configUrl = Components.classes[STDURL_CTRID].createInstance(nsIURI);
            this.configUrl.spec = fp.fileURL.spec;
            document.getElementById('config').setAttribute('value', this.configUrl.spec);
        }
        this.parseConfig();
        return true;
    },

    parseConfig: function()
    {
        this.testArray = new Array();
        var test;
        if (!this.configUrl) {
            return;
        }

        var content = loadFile(this.configUrl.spec);

        var lines = content.split("\n");
        var line, res;
        var head = /^\[(.+)\]$/;
        var instruct = /^(.+)=(.+)$/;
        while (lines.length) {
            line = lines.shift();
            if (head.test(line)) {
                test = new Object;
                res = head.exec(line);
                test['title'] = res[1];
                this.testArray.push(test);
            }
            else if (line == '') {
                test = undefined;
            }
            else {
                res = instruct.exec(line);
                test[res[1]] = res[2];
            }
        }
    },

    onLoad: function()
    {
        this.mCurrentStatus = document.getElementById('currentStatus');
        this.mCurrentProgress = document.getElementById('currentProgress');
        this.mTotalProgress = document.getElementById('totalProgress');
        this.mOutput = document.getElementById('transformOutput');
        this.mDetailOutput = 
            document.getElementById('transformDetailedOutput');
        this.mDetail = true;
    },

    progress: function(aTitle, aTime, aProgress)
    {
        // dump20(aTitle);
        // dump20(aTime);
        // dump20(aProgress);
        this.mCurrentProgress.value = aProgress;
        this.displayDetailTime(aTime);
        this.mTimes.push(aTime);
        // dump("\n");
    },

    done: function(aTitle)
    {
        // dump(aTitle + " is finished.\n");
        this.mCurrent++;
        this.mCurrentProgress.value = 0;
        this.displayTotalTime();
        if (this.mCurrent >= this.testArray.length) {
            this.mTotalProgress.value = 0;
            this.mCurrentStatus.value = "done";
            return;
        }
        this.mTotalProgress.value = this.mCurrent*100/this.testArray.length;
        var test = this.testArray[this.mCurrent];
        enablePrivilege('UniversalXPConnect');
        this.displayTest(test.title);
        runTest(test.title, this.configUrl.resolve(test.input),
                this.configUrl.resolve(test.stylesheet),
                test.iterations, this);
    },

    onStop: function()
    {
        clearTimeout(gTimeout);
        this.mCurrentProgress.value = 0;
        this.mTotalProgress.value = 0;
        this.mCurrentStatus.value = "stopped";
    },

    displayTest: function(aTitle)
    {
        this.mTimes = new Array;
        aTitle += "\t";
        this.mCurrentStatus.value = aTitle;
        this.mOutput.value += aTitle;
        if (this.mDetail) {
            this.mDetailOutput.value += aTitle;
        }
    },

    displayDetailTime: function(aTime)
    {
        if (this.mDetail) {
            this.mDetailOutput.value += aTime + " ms\t";
        }
    },

    displayTotalTime: function()
    {
        var sum = 0;
        for (k = 0; k < this.mTimes.length; k++) {
            sum += this.mTimes[k];
        }
        var mean = sum / this.mTimes.length;
        this.mOutput.value += Number(mean).toFixed(2) + " ms\t" + sum + " ms\t";
        var variance = 0;
        for (k = 0; k < this.mTimes.length; k++) {
            var n = this.mTimes[k] - mean;
            variance += n*n;
        }
        variance = Math.sqrt(variance/this.mTimes.length);
        this.mOutput.value += Number(variance).toFixed(2)+"\n";
        if (this.mDetail) {
            this.mDetailOutput.value += "\n";
        }
    },

    runBenchmark: function()
    {
        enablePrivilege('UniversalXPConnect');
        if (!this.testArray) {
            if (!this.configUrl) {
                this.configUrl = Components.classes[STDURL_CTRID].createInstance(nsIURI);
                this.configUrl.spec = document.getElementById('config').value;
            }
            this.parseConfig();
        }

        this.mCurrent = 0;
        var test = this.testArray[this.mCurrent];
        this.mOutput.value = '';
        if (this.mDetail) {
            this.mDetailOutput.value = '';
        }
        this.displayTest(test.title);
        runTest(test.title, this.configUrl.resolve(test.input),
                this.configUrl.resolve(test.stylesheet),
                test.iterations, this);
        return true;
    }
}

