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

var gParser = new DOMParser;
var gProc = new XSLTProcessor;
var gTimeout;

function Test(aTitle, aSourceURL, aStyleURL, aNumber, aObserver)
{
    this.mTitle = aTitle;
    this.mObserver = aObserver;
    this.mTotal = aNumber;
    this.mDone = 0;
    var xmlcontent = loadFile(aSourceURL);
    var xslcontent = loadFile(aStyleURL);
    this.mSource = gParser.parseFromString(xmlcontent, 'text/xml');
    this.mStyle = gParser.parseFromString(xslcontent, 'text/xml');
}

function runTest(aTitle, aSourceURL, aStyleURL, aNumber, aObserver)
{
    test = new Test(aTitle, aSourceURL, aStyleURL, aNumber,
                        aObserver);
    gTimeout = setTimeout(onNextTransform, 100, test, 0);
}

function onNextTransform(aTest, aNumber)
{
    res = document.implementation.createDocument('', '', null);
    var startTime = Date.now();
    gProc.transformDocument(aTest.mSource, aTest.mStyle, res, null);
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
