/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/******

  This file contains the list of all text services nsIAtoms and their values
  
  It is designed to be used as inline input to nsTextServicesDocument.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro TS_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to TS_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

// OUTPUT_CLASS=nsTextServicesDocument
// MACRO_NAME=TS_ATOM

TS_ATOM(sAAtom, "a")
TS_ATOM(sAddressAtom, "address")
TS_ATOM(sBigAtom, "big")
TS_ATOM(sBAtom, "b")
TS_ATOM(sCiteAtom, "cite")
TS_ATOM(sCodeAtom, "code")
TS_ATOM(sDfnAtom, "dfn")
TS_ATOM(sEmAtom, "em")
TS_ATOM(sFontAtom, "font")
TS_ATOM(sIAtom, "i")
TS_ATOM(sKbdAtom, "kbd")
TS_ATOM(sKeygenAtom, "keygen")
TS_ATOM(sNobrAtom, "nobr")
TS_ATOM(sSAtom, "s")
TS_ATOM(sSampAtom, "samp")
TS_ATOM(sSmallAtom, "small")
TS_ATOM(sSpacerAtom, "spacer")
TS_ATOM(sSpanAtom, "span")
TS_ATOM(sStrikeAtom, "strike")
TS_ATOM(sStrongAtom, "strong")
TS_ATOM(sSubAtom, "sub")
TS_ATOM(sSupAtom, "sup")
TS_ATOM(sTtAtom, "tt")
TS_ATOM(sUAtom, "u")
TS_ATOM(sVarAtom, "var")
TS_ATOM(sWbrAtom, "wbr")
