/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Lists of valid & invalid values for the various <animateMotion> attributes */
const gValidValues = [
  "10 10",
  "10 10;",  // Trailing semicolons are allowed
  "10 10;  ",
  "   10   10em  ",
  "1 2  ; 3,4",
  "1,2;3,4",
  "0 0",
  "0,0",
];

const gInvalidValues = [
  ";10 10",
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

const gValidKeyPoints = [
  "0; 0.5; 1",
  "0;.5;1",
  "0; 0; 1",
  "0; 1; 1",
  "0; 0; 1;", // Trailing semicolons are allowed
  "0; 0; 1; ",
  "0; 0.000; 1",
  "0; 0.000001; 1",
];

// Should have 3 values to be valid.
// Same as number of keyTimes values
const gInvalidKeyPoints = [
  "0; 1",
  "0; 0.5; 0.75; 1",
  "0; 1;",
  "0",
  "1",
  "a",
  "",
  "  ",
  "0; -0.1; 1",
  "0; 1.1; 1",
  "0; 0.1; 1.1",
  "-0.1; 0.1; 1",
  "0; a; 1",
  "0;;1",
];
