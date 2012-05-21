// |reftest| random -- bogus perf test (bug 467263)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    14 Feb 2002
 * SUMMARY: Performance: Regexp performance degraded from 4.7
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=85721
 *
 * Adjust this testcase if necessary. The FAST constant defines
 * an upper bound in milliseconds for any execution to take.
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 85721;
var summary = 'Performance: execution of regular expression';
var FAST = 100; // execution should be 100 ms or less to pass the test
var MSG_FAST = 'Execution took less than ' + FAST + ' ms';
var MSG_SLOW = 'Execution took ';
var MSG_MS = ' ms';
var str = '';
var re = '';
var status = '';
var actual = '';
var expect= '';

printBugNumber(BUGNUMBER);
printStatus (summary);


function elapsedTime(startTime)
{
  return new Date() - startTime;
}


function isThisFast(ms)
{
  if (ms <= FAST)
    return MSG_FAST;
  return MSG_SLOW + ms + MSG_MS;
}



/*
 * The first regexp. We'll test for performance (Section 1) and accuracy (Section 2).
 */
str='<sql:connection id="conn1"> <sql:url>www.m.com</sql:url> <sql:driver>drive.class</sql:driver>\n<sql:userId>foo</sql:userId> <sql:password>goo</sql:password> </sql:connection>';
re = /<sql:connection id="([^\r\n]*?)">\s*<sql:url>\s*([^\r\n]*?)\s*<\/sql:url>\s*<sql:driver>\s*([^\r\n]*?)\s*<\/sql:driver>\s*(\s*<sql:userId>\s*([^\r\n]*?)\s*<\/sql:userId>\s*)?\s*(\s*<sql:password>\s*([^\r\n]*?)\s*<\/sql:password>\s*)?\s*<\/sql:connection>/;
expect = Array("<sql:connection id=\"conn1\"> <sql:url>www.m.com</sql:url> <sql:driver>drive.class</sql:driver>\n<sql:userId>foo</sql:userId> <sql:password>goo</sql:password> </sql:connection>","conn1","www.m.com","drive.class","<sql:userId>foo</sql:userId> ","foo","<sql:password>goo</sql:password> ","goo");

/*
 *  Check performance -
 */
status = inSection(1);
var start = new Date();
var result = re.exec(str);
actual = elapsedTime(start);
reportCompare(isThisFast(FAST), isThisFast(actual), status);

/*
 *  Check accuracy -
 */
status = inSection(2);
testRegExp([status], [re], [str], [result], [expect]);



/*
 * The second regexp (HUGE!). We'll test for performance (Section 3) and accuracy (Section 4).
 * It comes from the O'Reilly book "Mastering Regular Expressions" by Jeffrey Friedl, Appendix B
 */

//# Some things for avoiding backslashitis later on.
$esc        = '\\\\';     
$Period      = '\.';
$space      = '\040';              $tab         = '\t';
$OpenBR     = '\\[';               $CloseBR     = '\\]';
$OpenParen  = '\\(';               $CloseParen  = '\\)';
$NonASCII   = '\x80-\xff';         $ctrl        = '\000-\037';
$CRlist     = '\n\015';  //# note: this should really be only \015.
// Items 19, 20, 21
$qtext = '[^' + $esc + $NonASCII + $CRlist + '\"]';						  // # for within "..."
$dtext = '[^' + $esc + $NonASCII + $CRlist + $OpenBR + $CloseBR + ']';    // # for within [...]
$quoted_pair = $esc + '[^' + $NonASCII + ']';							  // # an escaped character

//##############################################################################
//# Items 22 and 23, comment.
//# Impossible to do properly with a regex, I make do by allowing at most one level of nesting.
$ctext   =  '[^' + $esc + $NonASCII + $CRlist + '()]';

//# $Cnested matches one non-nested comment.
//# It is unrolled, with normal of $ctext, special of $quoted_pair.
$Cnested =
  $OpenParen +                                 // #  (
  $ctext + '*' +                            // #     normal*
  '(?:' + $quoted_pair + $ctext + '*)*' +   // #     (special normal*)*
  $CloseParen;                                 // #                       )


//# $comment allows one level of nested parentheses
//# It is unrolled, with normal of $ctext, special of ($quoted_pair|$Cnested)
$comment =
  $OpenParen +                                           // #  (
  $ctext + '*' +                                     // #     normal*
  '(?:' +                                            // #       (
  '(?:' + $quoted_pair + '|' + $Cnested + ')' +   // #         special
  $ctext + '*' +                                 // #         normal*
  ')*' +                                             // #            )*
  $CloseParen;                                           // #                )


//##############################################################################
//# $X is optional whitespace/comments.
$X =
  '[' + $space + $tab + ']*' +					       // # Nab whitespace.
  '(?:' + $comment + '[' + $space + $tab + ']*)*';    // # If comment found, allow more spaces.


//# Item 10: atom
$atom_char   = '[^(' + $space + '<>\@,;:\".' + $esc + $OpenBR + $CloseBR + $ctrl + $NonASCII + ']';
$atom =
  $atom_char + '+' +            // # some number of atom characters...
  '(?!' + $atom_char + ')';     // # ..not followed by something that could be part of an atom

// # Item 11: doublequoted string, unrolled.
$quoted_str =
  '\"' +                                         // # "
  $qtext + '*' +                              // #   normal
  '(?:' + $quoted_pair + $qtext + '*)*' +     // #   ( special normal* )*
  '\"';                                          // # "

//# Item 7: word is an atom or quoted string
$word =
  '(?:' +
  $atom +                // # Atom
  '|' +                  //     #  or
  $quoted_str +          // # Quoted string
  ')'

//# Item 12: domain-ref is just an atom
  $domain_ref  = $atom;

//# Item 13: domain-literal is like a quoted string, but [...] instead of  "..."
$domain_lit  =
  $OpenBR +								   	     // # [
  '(?:' + $dtext + '|' + $quoted_pair + ')*' +     // #    stuff
  $CloseBR;                                        // #           ]

// # Item 9: sub-domain is a domain-ref or domain-literal
$sub_domain  =
  '(?:' +
  $domain_ref +
  '|' +
  $domain_lit +
  ')' +
  $X;                 // # optional trailing comments

// # Item 6: domain is a list of subdomains separated by dots.
$domain =
  $sub_domain +
  '(?:' +
  $Period + $X + $sub_domain +
  ')*';

//# Item 8: a route. A bunch of "@ $domain" separated by commas, followed by a colon.
$route =
  '\@' + $X + $domain +
  '(?:,' + $X + '\@' + $X + $domain + ')*' +  // # additional domains
  ':' +
  $X;					// # optional trailing comments

//# Item 6: local-part is a bunch of $word separated by periods
$local_part =
  $word + $X
  '(?:' +
  $Period + $X + $word + $X +		// # additional words
  ')*';

// # Item 2: addr-spec is local@domain
$addr_spec  =
  $local_part + '\@' + $X + $domain;

//# Item 4: route-addr is <route? addr-spec>
$route_addr =
  '<' + $X +                     // # <
  '(?:' + $route + ')?' +     // #       optional route
  $addr_spec +                // #       address spec
  '>';                           // #                 >

//# Item 3: phrase........
$phrase_ctrl = '\000-\010\012-\037'; // # like ctrl, but without tab

//# Like atom-char, but without listing space, and uses phrase_ctrl.
//# Since the class is negated, this matches the same as atom-char plus space and tab
$phrase_char =
  '[^()<>\@,;:\".' + $esc + $OpenBR + $CloseBR + $NonASCII + $phrase_ctrl + ']';

// # We've worked it so that $word, $comment, and $quoted_str to not consume trailing $X
// # because we take care of it manually.
$phrase =
  $word +                                                  // # leading word
  $phrase_char + '*' +                                     // # "normal" atoms and/or spaces
  '(?:' +
  '(?:' + $comment + '|' + $quoted_str + ')' +          // # "special" comment or quoted string
  $phrase_char + '*' +                                  // #  more "normal"
  ')*';

// ## Item #1: mailbox is an addr_spec or a phrase/route_addr
$mailbox =
  $X +                                // # optional leading comment
  '(?:' +
  $phrase + $route_addr +     // # name and address
  '|' +                       //     #  or
  $addr_spec +                // # address
  ')';


//###########################################################################


re = new RegExp($mailbox, "g");
str = 'Jeffy<"That Tall Guy"@ora.com (this address is no longer active)>';
expect = Array('Jeffy<"That Tall Guy"@ora.com (this address is no longer active)>');

/*
 *  Check performance -
 */
status = inSection(3);
var start = new Date();
var result = re.exec(str);
actual = elapsedTime(start);
reportCompare(isThisFast(FAST), isThisFast(actual), status);

/*
 *  Check accuracy -
 */
status = inSection(4);
testRegExp([status], [re], [str], [result], [expect]);
