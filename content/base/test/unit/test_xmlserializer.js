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
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is Disruptive Innovations.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>, Original author
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

function xmlEncode(aFile, aFlags, aCharset) {
    if(aFlags == undefined) aFlags = 0;
    if(aCharset == undefined) aCharset = "UTF-8";

    var doc = do_parse_document(aFile, "text/xml");

    var encoder = Components.classes["@mozilla.org/layout/documentEncoder;1?type=text/xml"]
                   .createInstance(nsIDocumentEncoder);
    encoder.setCharset(aCharset);
    encoder.init(doc, "text/xml", aFlags);
    return encoder.encodeToString();
}

function run_test()
{
    var result, expected;
    const de = Components.interfaces.nsIDocumentEncoder;

    result = xmlEncode("1_original.xml", de.OutputLFLineBreak);
    expected =loadContentFile("1_result.xml");
    do_check_eq(expected, result);

    result =  xmlEncode("2_original.xml", de.OutputLFLineBreak);
    expected = loadContentFile("2_result_1.xml");
    do_check_eq(expected, result);

    result =  xmlEncode("2_original.xml", de.OutputCRLineBreak);
    expected = expected.replace(/\n/g, "\r");
    do_check_eq(expected, result);

    result = xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputCRLineBreak);
    expected = expected.replace(/\r/mg, "\r\n");
    do_check_eq(expected, result);

    result =  xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputFormatted);
    expected = loadContentFile("2_result_2.xml");
    do_check_eq(expected, result);

    result =  xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    expected = loadContentFile("2_result_3.xml");
    do_check_eq(expected, result);

    result =  xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputWrap);
    expected = loadContentFile("2_result_4.xml");
    do_check_eq(expected, result);

    result =  xmlEncode("3_original.xml", de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    expected = loadContentFile("3_result.xml");
    do_check_eq(expected, result);

    result =  xmlEncode("3_original.xml", de.OutputLFLineBreak | de.OutputWrap);
    expected = loadContentFile("3_result_2.xml");
    do_check_eq(expected, result);

    // tests on namespaces
    var doc = do_parse_document("4_original.xml", "text/xml");

    var encoder = Components.classes["@mozilla.org/layout/documentEncoder;1?type=text/xml"]
                   .createInstance(nsIDocumentEncoder);
    encoder.setCharset("UTF-8");
    encoder.init(doc, "text/xml", de.OutputLFLineBreak);

    result = encoder.encodeToString();
    expected = loadContentFile("4_result_1.xml");
    do_check_eq(expected, result);

    encoder.setNode(doc.documentElement.childNodes[9]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_2.xml");
    do_check_eq(expected, result);

    encoder.setNode(doc.documentElement.childNodes[7].childNodes[1]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_3.xml");
    do_check_eq(expected, result);

    encoder.setNode(doc.documentElement.childNodes[11].childNodes[1]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_4.xml");
    do_check_eq(expected, result);

    encoder.setCharset("UTF-8");
    // it doesn't support this flags
    encoder.init(doc, "text/xml",  de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    encoder.setWrapColumn(40);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_5.xml");
    do_check_eq(expected, result);

    encoder.init(doc, "text/xml",  de.OutputLFLineBreak | de.OutputWrap);
    encoder.setWrapColumn(40);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_6.xml");
    do_check_eq(expected, result);

}
