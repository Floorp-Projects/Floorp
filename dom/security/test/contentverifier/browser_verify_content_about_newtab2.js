
const TESTS = [
  // { newtab (aboutURI) or regular load (url) : url,
  //   testStrings : expected strings in the loaded page }
  { "aboutURI" : URI_GOOD, "testStrings" : [GOOD_ABOUT_STRING] },
  { "aboutURI" : URI_ERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_KEYERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_SIGERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_NO_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_SIG, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BROKEN_SIG, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_X5U, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_HTTP_X5U, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_FILE, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_ALL, "testStrings" : [ABOUT_BLANK] },
  { "url" : URI_CLEANUP, "testStrings" : [CLEANUP_DONE] },
];

add_task(runTests);
