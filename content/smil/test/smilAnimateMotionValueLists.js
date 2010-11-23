/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SMIL Test Code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Lists of valid & invalid values for the various <animateMotion> attributes */
const gValidValues = [
  "10 10",
  "   10   10em  ",
  "1 2  ; 3,4",
  "1,2;3,4",
  "0 0",
  "0,0",
];

const gInvalidValues = [
  ";10 10",
  "10 10;",  // We treat semicolon-terminated value-lists as failure cases
  "10 10;;",
  "1 2 3",
  "1 2 3 4",
  "1,2;3,4 ,",
  ",", " , ",
  ";", " ; ",
  "a", " a; ", ";a;",
  "", " ",
  "1,2;3,4,",
  "1,,2",
  ",1,2",
];

const gValidRotate = [
  "10",
  "20.1",
  "30.5deg",
  "0.5rad",
  "auto",
  "auto-reverse"
];

const gInvalidRotate = [
  " 10 ",
  "  10deg",
  "10 deg",
  "10deg ",
  "10 rad    ",
  "aaa",
  " 10.1 ",
];

const gValidToBy = [
 "0 0",
 "1em,2",
 "50.3em 0.2in",
 " 1,2",
 "1 2 "
];

const gInvalidToBy = [
 "0 0 0",
 "0 0,0",
 "0,0,0",
 "1emm 2",
 "1 2;",
 "1 2,",
 " 1,2 ,",
 "abc",
 ",",
 "",
 "1,,2",
 "1,2,"
];

const gValidPath = [
 "m0 0     L30 30",
 "M20,20L10    10",
 "M20,20 L30, 30h20",
 "m50 50", "M50 50",
 "m0 0", "M0, 0"
];

// paths must start with at least a valid "M" segment to be valid
const gInvalidPath = [
 "M20in 20",
 "h30",
 "L50 50",
 "abc",
];

// paths that at least start with a valid "M" segment are valid - the spec says
// to parse everything up to the first invalid token
const gValidPathWithErrors = [
 "M20 20em",
 "m0 0 L30,,30",
 "M10 10 L50 50 abc",
];
