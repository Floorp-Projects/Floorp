/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_tokenizer() {
  let testContexts = [
    { desc: "Empty string",
      searchString: "test",
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
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
      searchString: "* test",
      expectedTokens: [
        { value: "*", type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
      ],
    },
    { desc: "separate restriction char at end",
      searchString: "test *",
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "*", type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
      ],
    },
    { desc: "boundary restriction char at end",
      searchString: "test*",
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "*", type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
      ],
    },
    { desc: "double boundary restriction char",
      searchString: "*test#",
      expectedTokens: [
        { value: "*", type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: "test#", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "double non-combinable restriction char, single char string",
      searchString: "t*?",
      expectedTokens: [
        { value: "t*", type: UrlbarTokenizer.TYPE.TEXT },
        { value: "?", type: UrlbarTokenizer.TYPE.RESTRICT_SEARCH },
      ],
    },
    { desc: "only boundary restriction chars",
      searchString: "*#",
      expectedTokens: [
        { value: "*", type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
        { value: "#", type: UrlbarTokenizer.TYPE.RESTRICT_TITLE },
      ],
    },
    { desc: "only the boundary restriction char",
      searchString: "*",
      expectedTokens: [
        { value: "*", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "boundary restriction char on path",
      searchString: "test/#",
      expectedTokens: [
        { value: "test/#", type: UrlbarTokenizer.TYPE.POSSIBLE_URL },
      ],
    },
    { desc: "multiple boundary restriction chars suffix",
      searchString: "test ^ ~",
      expectedTokens: [
        { value: "test", type: UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN },
        { value: "^", type: UrlbarTokenizer.TYPE.RESTRICT_HISTORY },
        { value: "~", type: UrlbarTokenizer.TYPE.TEXT },
      ],
    },
    { desc: "multiple boundary restriction chars prefix",
      searchString: "^ ~ test",
      expectedTokens: [
        { value: "^", type: UrlbarTokenizer.TYPE.RESTRICT_HISTORY },
        { value: "~", type: UrlbarTokenizer.TYPE.TEXT },
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
      searchString: "* 192.168.1.1:8",
      expectedTokens: [
        { value: "*", type: UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK },
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
    { desc: "bogus protocol",
      searchString: "http:///",
      expectedTokens: [
        { value: "http:///", type: UrlbarTokenizer.TYPE.TEXT },
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
  ];

  for (let queryContext of testContexts) {
    info(queryContext.desc);
    let newQueryContext = UrlbarTokenizer.tokenize(queryContext);
    Assert.equal(queryContext, newQueryContext,
                 "The queryContext object is the same");
    Assert.deepEqual(queryContext.tokens, queryContext.expectedTokens,
                     "Check the expected tokens");
  }
});
