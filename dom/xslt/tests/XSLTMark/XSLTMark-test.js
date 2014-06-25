/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gParser = new DOMParser;
var gTimeout;

function Test(aTitle, aSourceURL, aStyleURL, aNumber, aObserver)
{
    this.mTitle = aTitle;
    this.mObserver = aObserver;
    this.mTotal = aNumber;
    this.mDone = 0;
    var xmlcontent = loadFile(aSourceURL);
    var xslcontent = loadFile(aStyleURL);
    this.mSource = gParser.parseFromString(xmlcontent, 'application/xml');
    this.mStyle = gParser.parseFromString(xslcontent, 'application/xml');
}

function runTest(aTitle, aSourceURL, aStyleURL, aNumber, aObserver)
{
    test = new Test(aTitle, aSourceURL, aStyleURL, aNumber,
                        aObserver);
    gTimeout = setTimeout(onNextTransform, 100, test, 0);
}

function onNextTransform(aTest, aNumber)
{
    var proc = new XSLTProcessor;
    var startTime = Date.now();
    proc.importStylesheet(aTest.mStyle);
    var res = proc.transformToDocument(aTest.mSource);
    var endTime = Date.now();
    aNumber++;
    var progress = aNumber / aTest.mTotal * 100;
    if (aTest.mObserver) {
        aTest.mObserver.progress(aTest.mTitle, endTime - startTime,
                                 progress);
    }
    if (aNumber < aTest.mTotal) {
        gTimeout = setTimeout(onNextTransform, 100, aTest, aNumber);
    } else if (aTest.mObserver) {
        aTest.mObserver.done(aTest.mTitle);
    }
}
