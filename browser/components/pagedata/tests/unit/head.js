/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.sys.mjs",
});

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const server = new HttpServer();
server.start(-1);

const SERVER_PORT = server.identity.primaryPort;
const BASE_URL = "http://localhost:" + SERVER_PORT;
const DEFAULT_PATH = "/document.html";
const TEST_URL = BASE_URL + DEFAULT_PATH;

registerCleanupFunction(() => {
  server.stop();
});

do_get_profile();
Services.prefs.setBoolPref("browser.pagedata.log", true);

/**
 * Given a string parses it as HTML into a DOM Document object.
 *
 * @param {string} str
 *   The string to parse.
 * @param {string} path
 *   The path for the document on the server, defaults to "/document.html"
 * @returns {Promise<Document>} the HTML DOM Document object.
 */
function parseDocument(str, path = DEFAULT_PATH) {
  server.registerPathHandler(path, (request, response) => {
    response.setHeader("Content-Type", "text/html;charset=utf-8");

    let converter = Cc[
      "@mozilla.org/intl/converter-output-stream;1"
    ].createInstance(Ci.nsIConverterOutputStream);
    converter.init(response.bodyOutputStream, "utf-8");
    converter.writeString(str);
  });

  return new Promise((resolve, reject) => {
    let request = new XMLHttpRequest();
    request.responseType = "document";
    request.open("GET", BASE_URL + path, true);

    request.addEventListener("error", reject);
    request.addEventListener("abort", reject);

    request.addEventListener("load", function() {
      resolve(request.responseXML);
    });

    request.send();
  });
}

/**
 * Parses page data from a HTML string.
 *
 * @param {string} str
 *   The HTML string to parse.
 * @param {string} path
 *   The path for the document on the server, defaults to "/document.html"
 * @returns {Promise<PageData>} A promise that resolves to the page data found.
 */
async function parsePageData(str, path) {
  let doc = await parseDocument(str, path);
  return PageDataSchema.collectPageData(doc);
}

/**
 * Verifies that the HTML string given parses to the expected page data.
 *
 * @param {string} str
 *   The HTML string to parse.
 * @param {PageData} expected
 *   The expected pagedata excluding the date and url properties.
 * @param {string} path
 *   The path for the document on the server, defaults to "/document.html"
 * @returns {Promise<PageData>} A promise that resolves to the page data found.
 */
async function verifyPageData(str, expected, path = DEFAULT_PATH) {
  let pageData = await parsePageData(str, path);

  delete pageData.date;

  Assert.equal(pageData.url, BASE_URL + path);
  delete pageData.url;

  Assert.deepEqual(
    pageData,
    expected,
    "Should have seen the expected page data."
  );
}
