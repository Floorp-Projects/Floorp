/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Components.utils.import("resource:///modules/TelURIParser.jsm")

  // blocked numbers
  do_check_eq(TelURIParser.parseURI('tel', 'tel:#1234*'), null);
  do_check_eq(TelURIParser.parseURI('tel', 'tel:*1234#'), null);
  do_check_eq(TelURIParser.parseURI('tel', 'tel:*1234*'), null);
  do_check_eq(TelURIParser.parseURI('tel', 'tel:#1234#'), null);
  do_check_eq(TelURIParser.parseURI('tel', 'tel:*#*#7780#*#*'), null);
  do_check_eq(TelURIParser.parseURI('tel', 'tel:*1234AB'), null);

  // white list
  do_check_eq(TelURIParser.parseURI('tel', 'tel:*1234'), '*1234');
  do_check_eq(TelURIParser.parseURI('tel', 'tel:#1234'), '#1234');
}
