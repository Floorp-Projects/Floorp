function getPostDataString(aIS) {
  if (!aIS)
    return null;

  var sis = Cc["@mozilla.org/scriptableinputstream;1"].
            createInstance(Ci.nsIScriptableInputStream);
  sis.init(aIS);
  var dataLines = sis.read(aIS.available()).split("\n");

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
  init: function(aKeyWord, aURL, aPostData, aSearchWord) {
    this.keyword = aKeyWord;
    this.uri = makeURI(aURL);
    this.postData = aPostData;
    this.searchWord = aSearchWord;

    this.method = (this.postData ? "POST" : "GET");
  }
}

function bmKeywordData(aKeyWord, aURL, aPostData, aSearchWord) {
  this.init(aKeyWord, aURL, aPostData, aSearchWord);
}
bmKeywordData.prototype = new keyWordData();

function searchKeywordData(aKeyWord, aURL, aPostData, aSearchWord) {
  this.init(aKeyWord, aURL, aPostData, aSearchWord);
}
searchKeywordData.prototype = new keyWordData();

var testData = [
  [new bmKeywordData("bmget", "http://bmget/search=%s", null, "foo"),
   new keywordResult("http://bmget/search=foo", null)],

  [new bmKeywordData("bmpost", "http://bmpost/", "search=%s", "foo2"),
   new keywordResult("http://bmpost/", "search=foo2")],

  [new bmKeywordData("bmpostget", "http://bmpostget/search1=%s", "search2=%s", "foo3"),
   new keywordResult("http://bmpostget/search1=foo3", "search2=foo3")],

  [new bmKeywordData("bmget-nosearch", "http://bmget-nosearch/", null, ""),
   new keywordResult("http://bmget-nosearch/", null)],

  [new searchKeywordData("searchget", "http://searchget/?search={searchTerms}", null, "foo4"),
   new keywordResult("http://searchget/?search=foo4", null, true)],

  [new searchKeywordData("searchpost", "http://searchpost/", "search={searchTerms}", "foo5"),
   new keywordResult("http://searchpost/", "search=foo5", true)],

  [new searchKeywordData("searchpostget", "http://searchpostget/?search1={searchTerms}", "search2={searchTerms}", "foo6"),
   new keywordResult("http://searchpostget/?search1=foo6", "search2=foo6", true)],

  // Bookmark keywords that don't take parameters should not be activated if a
  // parameter is passed (bug 420328).
  [new bmKeywordData("bmget-noparam", "http://bmget-noparam/", null, "foo7"),
   new keywordResult(null, null, true)],
  [new bmKeywordData("bmpost-noparam", "http://bmpost-noparam/", "not_a=param", "foo8"),
   new keywordResult(null, null, true)],

  // Test escaping (%s = escaped, %S = raw)
  // UTF-8 default
  [new bmKeywordData("bmget-escaping", "http://bmget/?esc=%s&raw=%S", null, "foé"),
   new keywordResult("http://bmget/?esc=fo%C3%A9&raw=foé", null)],
  // Explicitly-defined ISO-8859-1
  [new bmKeywordData("bmget-escaping2", "http://bmget/?esc=%s&raw=%S&mozcharset=ISO-8859-1", null, "foé"),
   new keywordResult("http://bmget/?esc=fo%E9&raw=foé", null)],

  // Bug 359809: Test escaping +, /, and @
  // UTF-8 default
  [new bmKeywordData("bmget-escaping", "http://bmget/?esc=%s&raw=%S", null, "+/@"),
   new keywordResult("http://bmget/?esc=%2B%2F%40&raw=+/@", null)],
  // Explicitly-defined ISO-8859-1
  [new bmKeywordData("bmget-escaping2", "http://bmget/?esc=%s&raw=%S&mozcharset=ISO-8859-1", null, "+/@"),
   new keywordResult("http://bmget/?esc=%2B%2F%40&raw=+/@", null)],

  // Test using a non-bmKeywordData object, to test the behavior of
  // getShortcutOrURIAndPostData for non-keywords (setupKeywords only adds keywords for
  // bmKeywordData objects)
  [{keyword: "http://gavinsharp.com"},
   new keywordResult(null, null, true)]
];

add_task(function* test_getshortcutoruri() {
  yield setupKeywords();

  for (let item of testData) {
    let [data, result] = item;

    let query = data.keyword;
    if (data.searchWord)
      query += " " + data.searchWord;
    let returnedData = yield getShortcutOrURIAndPostData(query);
    // null result.url means we should expect the same query we sent in
    let expected = result.url || query;
    is(returnedData.url, expected, "got correct URL for " + data.keyword);
    is(getPostDataString(returnedData.postData), result.postData, "got correct postData for " + data.keyword);
    is(returnedData.mayInheritPrincipal, !result.isUnsafe, "got correct mayInheritPrincipal for " + data.keyword);
  }

  yield cleanupKeywords();
});

var folder = null;
var gAddedEngines = [];

function* setupKeywords() {
  folder = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                title: "keyword-test" });
  for (let item of testData) {
    let data = item[0];
    if (data instanceof bmKeywordData) {
      yield PlacesUtils.bookmarks.insert({ url: data.uri, parentGuid: folder.guid });
      yield PlacesUtils.keywords.insert({ keyword: data.keyword, url: data.uri.spec, postData: data.postData });
    }

    if (data instanceof searchKeywordData) {
      Services.search.addEngineWithDetails(data.keyword, "", data.keyword, "", data.method, data.uri.spec);
      let addedEngine = Services.search.getEngineByName(data.keyword);
      if (data.postData) {
        let [paramName, paramValue] = data.postData.split("=");
        addedEngine.addParam(paramName, paramValue, null);
      }
      gAddedEngines.push(addedEngine);
    }
  }
}

function* cleanupKeywords() {
  PlacesUtils.bookmarks.remove(folder);
  gAddedEngines.map(Services.search.removeEngine);
}
