/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Components.utils.import("resource:///modules/TelURIParser.jsm")

  // global-phone-number
   do_check_eq(TelURIParser.parseURI('tel', 'tel:+1234'), '+1234');

  // global-phone-number => ignored chars
  do_check_eq(TelURIParser.parseURI('tel', 'tel:+1234_123'), '+1234');

  // global-phone-number => visualSeparator + digits
  do_check_eq(TelURIParser.parseURI('tel', 'tel:+-.()1234567890'), '+-.()1234567890');

  // local-phone-number
  do_check_eq(TelURIParser.parseURI('tel', 'tel:1234'), '1234');

  // local-phone-number => visualSeparator + digits + dtmfDigits + pauseCharacter
  do_check_eq(TelURIParser.parseURI('tel', 'tel:-.()1234567890ABCDpw'), '-.()1234567890ABCDpw');

  // local-phone-number => visualSeparator + digits + dtmfDigits + pauseCharacter + ignored chars
  do_check_eq(TelURIParser.parseURI('tel', 'tel:-.()1234567890ABCDpw_'), '-.()1234567890ABCDpw');

  // local-phone-number => isdn-subaddress
  do_check_eq(TelURIParser.parseURI('tel', 'tel:123;isub=123'), '123');

  // local-phone-number => post-dial
  do_check_eq(TelURIParser.parseURI('tel', 'tel:123;postd=123'),  '123');

  // local-phone-number => prefix
  do_check_eq(TelURIParser.parseURI('tel', 'tel:123;phone-context=+0321'), '+0321123');

  // local-phone-number => isdn-subaddress + post-dial + prefix
  do_check_eq(TelURIParser.parseURI('tel', 'tel:123;isub=123;postd=123;phone-context=+0321'), '+0321123');
}
