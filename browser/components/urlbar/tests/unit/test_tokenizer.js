/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_tokenizer() {
  let testContexts = [
    { desc: "Empty string",
      searchString: "",
      expectedTokens: [],
    },
    { desc: "Spaces string",
      searchString: "      ",
      expectedTokens: [],
    },
    { desc: "Single word string",
      searchString: "test",
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "Multi word string with mixed whitespace types",
      searchString: " test1 test2\u1680test3\u2004test4\u1680",
      expectedTokens: [
        { value: "test1", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "test2", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "test3", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "test4", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "separate restriction char at beginning",
      searchString: `${UrlbarTokenizer.RESTRICT.BOOKMARK} test`,
      expectedTokens: [
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "separate restriction char at end",
      searchString: `test ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
      ],
    },
    { desc: "boundary restriction char at end",
      searchString: `test${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
      ],
    },
    { desc: "separate restriction char in the middle",
      searchString: `test ${UrlbarTokenizer.RESTRICT.BOOKMARK} test`,
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.TEXT },
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "restriction char in the middle",
      searchString: `test${UrlbarTokenizer.RESTRICT.BOOKMARK}test`,
      expectedTokens: [
        { value: `test${UrlbarTokenizer.RESTRICT.BOOKMARK}test`, type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "restriction char in the middle 2",
      searchString: `test${UrlbarTokenizer.RESTRICT.BOOKMARK} test`,
      expectedTokens: [
        { value: `test${UrlbarTokenizer.RESTRICT.BOOKMARK}`, type: UrlbarTokenizer.TYPE.TEXT },
        { value: `test`, type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "double boundary restriction char",
      searchString: `${UrlbarTokenizer.RESTRICT.BOOKMARK}test${UrlbarTokenizer.RESTRICT.TITLE}`,
      expectedTokens: [
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: `test${UrlbarTokenizer.RESTRICT.TITLE}`, type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "double non-combinable restriction char, single char string",
      searchString: `t${UrlbarTokenizer.RESTRICT.BOOKMARK}${UrlbarTokenizer.RESTRICT.SEARCH}`,
      expectedTokens: [
        { value: `t${UrlbarTokenizer.RESTRICT.BOOKMARK}`, type: UrlbarTokenizer.TYPE.TEXT },
        { value: UrlbarTokenizer.RESTRICT.SEARCH, type: UrlbarTokenizer.TYPE.RESTRICT_SEARCH },
      ],
    },
    { desc: "only boundary restriction chars",
      searchString: `${UrlbarTokenizer.RESTRICT.BOOKMARK}${UrlbarTokenizer.RESTRICT.TITLE}`,
      expectedTokens: [
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: UrlbarTokenizer.RESTRICT.TITLE, type: UrlbarTokenizer.TYPE.RESTRICT_TITLE },
      ],
    },
    { desc: "only the boundary restriction char",
      searchString: UrlbarTokenizer.RESTRICT.BOOKMARK,
      expectedTokens: [
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
      ],
    },
    // Some restriction chars may be # or ?, that are also valid path parts.
    // The next 2 tests will check we consider those as part of url paths.
    { desc: "boundary # char on path",
      searchString: "test/#",
      expectedTokens: [
        { value: "test/#", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "boundary ? char on path",
      searchString: "test/?",
      expectedTokens: [
        { value: "test/?", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "multiple boundary restriction chars suffix",
      searchString: `test ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.TAG}`,
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: UrlbarTokenizer.RESTRICT.HISTORY, type: UrlbarTokenizer.TYPE.TEXT },
        { value: UrlbarTokenizer.RESTRICT.TAG, type: UrlbarTokenizer.TYPE.RESTRICT_TAG },
      ],
    },
    { desc: "multiple boundary restriction chars prefix",
      searchString: `${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.TAG} test`,
      expectedTokens: [
        { value: UrlbarTokenizer.RESTRICT.HISTORY, type: UrlbarTokenizer.TYPE.RESTRICT_HISTORY },
        { value: UrlbarTokenizer.RESTRICT.TAG, type: UrlbarTokenizer.TYPE.TEXT },
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "Math with division",
      searchString: "3.6/1.2",
      expectedTokens: [
        { value: "3.6/1.2", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "ipv4 in bookmarks",
      searchString: `${UrlbarTokenizer.RESTRICT.BOOKMARK} 192.168.1.1:8`,
      expectedTokens: [
        { value: UrlbarTokenizer.RESTRICT.BOOKMARK, type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: "192.168.1.1:8", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "email",
      searchString: "test@mozilla.com",
      expectedTokens: [
        { value: "test@mozilla.com", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "protocol",
      searchString: "http://test",
      expectedTokens: [
        { value: "http://test", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "bogus protocol with host (we allow visits to http://///example.com)",
      searchString: "http:///test",
      expectedTokens: [
        { value: "http:///test", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "file protocol with path",
      searchString: "file:///home",
      expectedTokens: [
        { value: "file:///home", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "almost a protocol",
      searchString: "http:",
      expectedTokens: [
        { value: "http:", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "almost a protocol 2",
      searchString: "http:/",
      expectedTokens: [
        { value: "http:/", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "bogus protocol (we allow visits to http://///example.com)",
      searchString: "http:///",
      expectedTokens: [
        { value: "http:///", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "file protocol",
      searchString: "file:///",
      expectedTokens: [
        { value: "file:///", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "userinfo",
      searchString: "user:pass@test",
      expectedTokens: [
        { value: "user:pass@test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "domain",
      searchString: "www.mozilla.org",
      expectedTokens: [
        { value: "www.mozilla.org", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "data uri",
      searchString: "data:text/plain,Content",
      expectedTokens: [
        { value: "data:text/plain,Content", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "ipv6",
      searchString: "[2001:db8::1]",
      expectedTokens: [
        { value: "[2001:db8::1]", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "numeric domain",
      searchString: "test1001.com",
      expectedTokens: [
        { value: "test1001.com", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "invalid ip",
      searchString: "192.2134.1.2",
      expectedTokens: [
        { value: "192.2134.1.2", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "ipv4",
      searchString: "1.2.3.4",
      expectedTokens: [
        { value: "1.2.3.4", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "host/path",
      searchString: "test/test",
      expectedTokens: [
        { value: "test/test", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "percent encoded string",
      searchString: "%E6%97%A5%E6%9C%AC",
      expectedTokens: [
        { value: "%E6%97%A5%E6%9C%AC", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "Uppercase",
      searchString: "TEST",
      expectedTokens: [
        { value: "TEST", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "Mixed case 1",
      searchString: "TeSt",
      expectedTokens: [
        { value: "TeSt", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "Mixed case 2",
      searchString: "tEsT",
      expectedTokens: [
        { value: "tEsT", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "Uppercase with spaces",
      searchString: "TEST EXAMPLE",
      expectedTokens: [
        { value: "TEST", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "EXAMPLE", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "Mixed case with spaces",
      searchString: "TeSt eXaMpLe",
      expectedTokens: [
        { value: "TeSt", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "eXaMpLe", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
  ];

  for (let queryContext of testContexts) {
    info(queryContext.desc);
    for (let token of queryContext.expectedTokens) {
      token.lowerCaseValue = token.value.toLocaleLowerCase();
    }
    let newQueryContext = UrlbarTokenizer.tokenize(queryContext);
    Assert.equal(queryContext, newQueryContext,
                 "The queryContext object is the same");
    Assert.deepEqual(queryContext.tokens, queryContext.expectedTokens,
                     "Check the expected tokens");
  }
});
