/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function xmlEncode(aFile, aFlags, aCharset) {
    if(aFlags == undefined) aFlags = 0;
    if(aCharset == undefined) aCharset = "UTF-8";

    var doc = await do_parse_document(aFile, "text/xml");

    var encoder = Cu.createDocumentEncoder("text/xml");
    encoder.setCharset(aCharset);
    encoder.init(doc, "text/xml", aFlags);
    return encoder.encodeToString();
}

add_task(async function() {
    var result, expected;
    const de = Ci.nsIDocumentEncoder;

    result = await xmlEncode("1_original.xml", de.OutputLFLineBreak);
    expected = loadContentFile("1_result.xml");
    Assert.equal(expected, result);

    result = await xmlEncode("2_original.xml", de.OutputLFLineBreak);
    expected = loadContentFile("2_result_1.xml");
    Assert.equal(expected, result);

    result = await xmlEncode("2_original.xml", de.OutputCRLineBreak);
    expected = expected.replace(/\n/g, "\r");
    Assert.equal(expected, result);

    result = await xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputCRLineBreak);
    expected = expected.replace(/\r/mg, "\r\n");
    Assert.equal(expected, result);

    result = await xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputFormatted);
    expected = loadContentFile("2_result_2.xml");
    Assert.equal(expected, result);

    result = await xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    expected = loadContentFile("2_result_3.xml");
    Assert.equal(expected, result);

    result = await xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputWrap);
    expected = loadContentFile("2_result_4.xml");
    Assert.equal(expected, result);

    result = await xmlEncode("3_original.xml", de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    expected = loadContentFile("3_result.xml");
    Assert.equal(expected, result);

    result = await xmlEncode("3_original.xml", de.OutputLFLineBreak | de.OutputWrap);
    expected = loadContentFile("3_result_2.xml");
    Assert.equal(expected, result);

    // tests on namespaces
    var doc = await do_parse_document("4_original.xml", "text/xml");

    var encoder = Cu.createDocumentEncoder("text/xml");
    encoder.setCharset("UTF-8");
    encoder.init(doc, "text/xml", de.OutputLFLineBreak);

    result = encoder.encodeToString();
    expected = loadContentFile("4_result_1.xml");
    Assert.equal(expected, result);

    encoder.setNode(doc.documentElement.childNodes[9]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_2.xml");
    Assert.equal(expected, result);

    encoder.setNode(doc.documentElement.childNodes[7].childNodes[1]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_3.xml");
    Assert.equal(expected, result);

    encoder.setNode(doc.documentElement.childNodes[11].childNodes[1]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_4.xml");
    Assert.equal(expected, result);

    encoder.setCharset("UTF-8");
    // it doesn't support this flags
    encoder.init(doc, "text/xml",  de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    encoder.setWrapColumn(40);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_5.xml");
    Assert.equal(expected, result);

    encoder.init(doc, "text/xml",  de.OutputLFLineBreak | de.OutputWrap);
    encoder.setWrapColumn(40);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_6.xml");
    Assert.equal(expected, result);
});
