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
 * The Original Code is text services atom table.
 *
 * The Initial Developer of the Original Code is
 * Netscape communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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
TS_ATOM(sBlinkAtom, "blink")
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
