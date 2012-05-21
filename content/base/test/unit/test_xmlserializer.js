/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
