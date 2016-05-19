
const TESTS = [
  // { newtab (aboutURI) or regular load (url) : url,
  //   testStrings : expected strings in the loaded page }
  { "aboutURI" : URI_GOOD, "testStrings" : [GOOD_ABOUT_STRING] },
  { "aboutURI" : URI_ERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "url" : URI_BAD_FILE_CACHED, "testStrings" : [BAD_ABOUT_STRING] },
  { "aboutURI" : URI_BAD_FILE_CACHED, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_GOOD, "testStrings" : [GOOD_ABOUT_STRING] },
  { "aboutURI" : URI_SRI, "testStrings" : [
    STYLESHEET_WITHOUT_SRI_BLOCKED,
    STYLESHEET_WITH_SRI_LOADED,
    SCRIPT_WITHOUT_SRI_BLOCKED,
    SCRIPT_WITH_SRI_LOADED,
    ]},
  { "aboutURI" : URI_BAD_CSP, "testStrings" : [CSP_TEST_SUCCESS_STRING] },
  { "url" : URI_CLEANUP, "testStrings" : [CLEANUP_DONE] },
];

add_task(runTests);
