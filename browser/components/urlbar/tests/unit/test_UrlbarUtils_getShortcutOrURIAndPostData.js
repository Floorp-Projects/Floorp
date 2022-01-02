/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of UrlbarController by stubbing out the
 * model and providing stubs to be called.
 */

"use strict";

function getPostDataString(aIS) {
  if (!aIS) {
    return null;
  }

  let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  sis.init(aIS);
  let dataLines = sis.read(aIS.available()).split("\n");

  // only want the last line
  return dataLines[dataLines.length - 1];
}

function keywordResult(aURL, aPostData, aIsUnsafe) {
  this.url = aURL;
  this.postData = aPostData;
  this.isUnsafe = aIsUnsafe;
}

function keyWordData() {}
keyWordData.prototype = {
  init(aKeyWord, aURL, aPostData, aSearchWord) {
    this.keyword = aKeyWord;
    this.uri = Services.io.newURI(aURL);
    this.postData = aPostData;
    this.searchWord = aSearchWord;

    this.method = this.postData ? "POST" : "GET";
  },
};

function bmKeywordData(aKeyWord, aURL, aPostData, aSearchWord) {
  this.init(aKeyWord, aURL, aPostData, aSearchWord);
}
bmKeywordData.prototype = new keyWordData();

function searchKeywordData(aKeyWord, aURL, aPostData, aSearchWord) {
  this.init(aKeyWord, aURL, aPostData, aSearchWord);
}
searchKeywordData.prototype = new keyWordData();

var testData = [
  [
    new bmKeywordData("bmget", "https://bmget/search=%s", null, "foo"),
    new keywordResult("https://bmget/search=foo", null),
  ],

  [
    new bmKeywordData("bmpost", "https://bmpost/", "search=%s", "foo2"),
    new keywordResult("https://bmpost/", "search=foo2"),
  ],

  [
    new bmKeywordData(
      "bmpostget",
      "https://bmpostget/search1=%s",
      "search2=%s",
      "foo3"
    ),
    new keywordResult("https://bmpostget/search1=foo3", "search2=foo3"),
  ],

  [
    new bmKeywordData("bmget-nosearch", "https://bmget-nosearch/", null, ""),
    new keywordResult("https://bmget-nosearch/", null),
  ],

  [
    new searchKeywordData(
      "searchget",
      "https://searchget/?search={searchTerms}",
      null,
      "foo4"
    ),
    new keywordResult("https://searchget/?search=foo4", null, true),
  ],

  [
    new searchKeywordData(
      "searchpost",
      "https://searchpost/",
      "search={searchTerms}",
      "foo5"
    ),
    new keywordResult("https://searchpost/", "search=foo5", true),
  ],

  [
    new searchKeywordData(
      "searchpostget",
      "https://searchpostget/?search1={searchTerms}",
      "search2={searchTerms}",
      "foo6"
    ),
    new keywordResult(
      "https://searchpostget/?search1=foo6",
      "search2=foo6",
      true
    ),
  ],

  // Bookmark keywords that don't take parameters should not be activated if a
  // parameter is passed (bug 420328).
  [
    new bmKeywordData("bmget-noparam", "https://bmget-noparam/", null, "foo7"),
    new keywordResult(null, null, true),
  ],
  [
    new bmKeywordData(
      "bmpost-noparam",
      "https://bmpost-noparam/",
      "not_a=param",
      "foo8"
    ),
    new keywordResult(null, null, true),
  ],

  // Test escaping (%s = escaped, %S = raw)
  // UTF-8 default
  [
    new bmKeywordData(
      "bmget-escaping",
      "https://bmget/?esc=%s&raw=%S",
      null,
      "fo\xE9"
    ),
    new keywordResult("https://bmget/?esc=fo%C3%A9&raw=fo\xE9", null),
  ],
  // Explicitly-defined ISO-8859-1
  [
    new bmKeywordData(
      "bmget-escaping2",
      "https://bmget/?esc=%s&raw=%S&mozcharset=ISO-8859-1",
      null,
      "fo\xE9"
    ),
    new keywordResult("https://bmget/?esc=fo%E9&raw=fo\xE9", null),
  ],

  // Bug 359809: Test escaping +, /, and @
  // UTF-8 default
  [
    new bmKeywordData(
      "bmget-escaping",
      "https://bmget/?esc=%s&raw=%S",
      null,
      "+/@"
    ),
    new keywordResult("https://bmget/?esc=%2B%2F%40&raw=+/@", null),
  ],
  // Explicitly-defined ISO-8859-1
  [
    new bmKeywordData(
      "bmget-escaping2",
      "https://bmget/?esc=%s&raw=%S&mozcharset=ISO-8859-1",
      null,
      "+/@"
    ),
    new keywordResult("https://bmget/?esc=%2B%2F%40&raw=+/@", null),
  ],

  // Test using a non-bmKeywordData object, to test the behavior of
  // getShortcutOrURIAndPostData for non-keywords (setupKeywords only adds keywords for
  // bmKeywordData objects)
  [{ keyword: "https://gavinsharp.com" }, new keywordResult(null, null, true)],
];

add_task(async function test_getshortcutoruri() {
  await setupKeywords();

  for (let item of testData) {
    let [data, result] = item;

    let query = data.keyword;
    if (data.searchWord) {
      query += " " + data.searchWord;
    }
    let returnedData = await UrlbarUtils.getShortcutOrURIAndPostData(query);
    // null result.url means we should expect the same query we sent in
    let expected = result.url || query;
    Assert.equal(
      returnedData.url,
      expected,
      "got correct URL for " + data.keyword
    );
    Assert.equal(
      getPostDataString(returnedData.postData),
      result.postData,
      "got correct postData for " + data.keyword
    );
    Assert.equal(
      returnedData.mayInheritPrincipal,
      !result.isUnsafe,
      "got correct mayInheritPrincipal for " + data.keyword
    );
  }

  await cleanupKeywords();
});

var folder = null;

async function setupKeywords() {
  folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "keyword-test",
  });
  for (let item of testData) {
    let data = item[0];
    if (data instanceof bmKeywordData) {
      await PlacesUtils.bookmarks.insert({
        url: data.uri,
        parentGuid: folder.guid,
      });
      await PlacesUtils.keywords.insert({
        keyword: data.keyword,
        url: data.uri.spec,
        postData: data.postData,
      });
    }

    if (data instanceof searchKeywordData) {
      await SearchTestUtils.installSearchExtension({
        name: data.keyword,
        keyword: data.keyword,
        search_url: data.uri.spec,
        search_url_get_params: "",
        search_url_post_params: data.postData,
      });
    }
  }
}

async function cleanupKeywords() {
  await PlacesUtils.bookmarks.remove(folder);
}
