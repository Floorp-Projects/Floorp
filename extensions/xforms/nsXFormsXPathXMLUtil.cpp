/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  David Landwehr <dlandwehr@novell.com>
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

#include "nsXFormsXPathXMLUtil.h"

static const PRUint32 BASECHAR_RANGE_LENGTH = 404;
static const PRUnichar BASECHAR_RANGE[] = {
 L'\x0041',L'\x005a',L'\x0061',L'\x007a',L'\x00c0',L'\x00d6',L'\x00d8',L'\x00f6',L'\x00f8',L'\x00ff',L'\x0100',L'\x0131',L'\x0134',L'\x013e',L'\x0141',L'\x0148',L'\x014a',L'\x017e',L'\x0180',L'\x01c3',L'\x01cd',L'\x01f0',L'\x01f4',L'\x01f5',L'\x01fa',L'\x0217',L'\x0250',L'\x02a8',L'\x02bb',L'\x02c1',L'\x0386',L'\x0386',L'\x0388',L'\x038a',L'\x038c',L'\x038c',L'\x038e',L'\x03a1',L'\x03a3',L'\x03ce'
,L'\x03d0',L'\x03d6',L'\x03da',L'\x03da',L'\x03dc',L'\x03dc',L'\x03de',L'\x03de',L'\x03e0',L'\x03e0',L'\x03e2',L'\x03f3',L'\x0401',L'\x040c',L'\x040e',L'\x044f',L'\x0451',L'\x045c',L'\x045e',L'\x0481',L'\x0490',L'\x04c4',L'\x04c7',L'\x04c8',L'\x04cb',L'\x04cc',L'\x04d0',L'\x04eb',L'\x04ee',L'\x04f5',L'\x04f8',L'\x04f9',L'\x0531',L'\x0556',L'\x0559',L'\x0559',L'\x0561',L'\x0586',L'\x05d0',L'\x05ea'
,L'\x05f0',L'\x05f2',L'\x0621',L'\x063a',L'\x0641',L'\x064a',L'\x0671',L'\x06b7',L'\x06ba',L'\x06be',L'\x06c0',L'\x06ce',L'\x06d0',L'\x06d3',L'\x06d5',L'\x06d5',L'\x06e5',L'\x06e6',L'\x0905',L'\x0939',L'\x093d',L'\x093d',L'\x0958',L'\x0961',L'\x0985',L'\x098c',L'\x098f',L'\x0990',L'\x0993',L'\x09a8',L'\x09aa',L'\x09b0',L'\x09b2',L'\x09b2',L'\x09b6',L'\x09b9',L'\x09dc',L'\x09dd',L'\x09df',L'\x09e1'
,L'\x09f0',L'\x09f1',L'\x0a05',L'\x0a0a',L'\x0a0f',L'\x0a10',L'\x0a13',L'\x0a28',L'\x0a2a',L'\x0a30',L'\x0a32',L'\x0a33',L'\x0a35',L'\x0a36',L'\x0a38',L'\x0a39',L'\x0a59',L'\x0a5c',L'\x0a5e',L'\x0a5e',L'\x0a72',L'\x0a74',L'\x0a85',L'\x0a8b',L'\x0a8d',L'\x0a8d',L'\x0a8f',L'\x0a91',L'\x0a93',L'\x0aa8',L'\x0aaa',L'\x0ab0',L'\x0ab2',L'\x0ab3',L'\x0ab5',L'\x0ab9',L'\x0abd',L'\x0abd',L'\x0ae0',L'\x0ae0'
,L'\x0b05',L'\x0b0c',L'\x0b0f',L'\x0b10',L'\x0b13',L'\x0b28',L'\x0b2a',L'\x0b30',L'\x0b32',L'\x0b33',L'\x0b36',L'\x0b39',L'\x0b3d',L'\x0b3d',L'\x0b5c',L'\x0b5d',L'\x0b5f',L'\x0b61',L'\x0b85',L'\x0b8a',L'\x0b8e',L'\x0b90',L'\x0b92',L'\x0b95',L'\x0b99',L'\x0b9a',L'\x0b9c',L'\x0b9c',L'\x0b9e',L'\x0b9f',L'\x0ba3',L'\x0ba4',L'\x0ba8',L'\x0baa',L'\x0bae',L'\x0bb5',L'\x0bb7',L'\x0bb9',L'\x0c05',L'\x0c0c'
,L'\x0c0e',L'\x0c10',L'\x0c12',L'\x0c28',L'\x0c2a',L'\x0c33',L'\x0c35',L'\x0c39',L'\x0c60',L'\x0c61',L'\x0c85',L'\x0c8c',L'\x0c8e',L'\x0c90',L'\x0c92',L'\x0ca8',L'\x0caa',L'\x0cb3',L'\x0cb5',L'\x0cb9',L'\x0cde',L'\x0cde',L'\x0ce0',L'\x0ce1',L'\x0d05',L'\x0d0c',L'\x0d0e',L'\x0d10',L'\x0d12',L'\x0d28',L'\x0d2a',L'\x0d39',L'\x0d60',L'\x0d61',L'\x0e01',L'\x0e2e',L'\x0e30',L'\x0e30',L'\x0e32',L'\x0e33'
,L'\x0e40',L'\x0e45',L'\x0e81',L'\x0e82',L'\x0e84',L'\x0e84',L'\x0e87',L'\x0e88',L'\x0e8a',L'\x0e8a',L'\x0e8d',L'\x0e8d',L'\x0e94',L'\x0e97',L'\x0e99',L'\x0e9f',L'\x0ea1',L'\x0ea3',L'\x0ea5',L'\x0ea5',L'\x0ea7',L'\x0ea7',L'\x0eaa',L'\x0eab',L'\x0ead',L'\x0eae',L'\x0eb0',L'\x0eb0',L'\x0eb2',L'\x0eb3',L'\x0ebd',L'\x0ebd',L'\x0ec0',L'\x0ec4',L'\x0f40',L'\x0f47',L'\x0f49',L'\x0f69',L'\x10a0',L'\x10c5'
,L'\x10d0',L'\x10f6',L'\x1100',L'\x1100',L'\x1102',L'\x1103',L'\x1105',L'\x1107',L'\x1109',L'\x1109',L'\x110b',L'\x110c',L'\x110e',L'\x1112',L'\x113c',L'\x113c',L'\x113e',L'\x113e',L'\x1140',L'\x1140',L'\x114c',L'\x114c',L'\x114e',L'\x114e',L'\x1150',L'\x1150',L'\x1154',L'\x1155',L'\x1159',L'\x1159',L'\x115f',L'\x1161',L'\x1163',L'\x1163',L'\x1165',L'\x1165',L'\x1167',L'\x1167',L'\x1169',L'\x1169'
,L'\x116d',L'\x116e',L'\x1172',L'\x1173',L'\x1175',L'\x1175',L'\x119e',L'\x119e',L'\x11a8',L'\x11a8',L'\x11ab',L'\x11ab',L'\x11ae',L'\x11af',L'\x11b7',L'\x11b8',L'\x11ba',L'\x11ba',L'\x11bc',L'\x11c2',L'\x11eb',L'\x11eb',L'\x11f0',L'\x11f0',L'\x11f9',L'\x11f9',L'\x1e00',L'\x1e9b',L'\x1ea0',L'\x1ef9',L'\x1f00',L'\x1f15',L'\x1f18',L'\x1f1d',L'\x1f20',L'\x1f45',L'\x1f48',L'\x1f4d',L'\x1f50',L'\x1f57'
,L'\x1f59',L'\x1f59',L'\x1f5b',L'\x1f5b',L'\x1f5d',L'\x1f5d',L'\x1f5f',L'\x1f7d',L'\x1f80',L'\x1fb4',L'\x1fb6',L'\x1fbc',L'\x1fbe',L'\x1fbe',L'\x1fc2',L'\x1fc4',L'\x1fc6',L'\x1fcc',L'\x1fd0',L'\x1fd3',L'\x1fd6',L'\x1fdb',L'\x1fe0',L'\x1fec',L'\x1ff2',L'\x1ff4',L'\x1ff6',L'\x1ffc',L'\x2126',L'\x2126',L'\x212a',L'\x212b',L'\x212e',L'\x212e',L'\x2180',L'\x2182',L'\x3041',L'\x3094',L'\x30a1',L'\x30fa'
,L'\x3105',L'\x312c',L'\xac00',L'\xd7a3'};

static const PRUint32 IDEOGRAPHIC_RANGE_LENGTH = 6;
static const PRUnichar IDEOGRAPHIC_RANGE[] = {
 L'\x4e00',L'\x9fa5',L'\x3007',L'\x3007',L'\x3021',L'\x3029'
};

static const PRUint32 COMBINING_CHAR_RANGE_LENGTH = 190;
static const PRUnichar COMBINING_CHAR_RANGE[] = {
 L'\x0300',L'\x0345',L'\x0360',L'\x0361',L'\x0483',L'\x0486',L'\x0591',L'\x05a1',L'\x05a3',L'\x05b9',L'\x05bb',L'\x05bd',L'\x05bf',L'\x05bf',L'\x05c1',L'\x05c2',L'\x05c4',L'\x05c4',L'\x064b',L'\x0652',L'\x0670',L'\x0670',L'\x06d6',L'\x06dc',L'\x06dd',L'\x06df',L'\x06e0',L'\x06e4',L'\x06e7',L'\x06e8',L'\x06ea',L'\x06ed',L'\x0901',L'\x0903',L'\x093c',L'\x093c',L'\x093e',L'\x094c',L'\x094d',L'\x094d'
,L'\x0951',L'\x0954',L'\x0962',L'\x0963',L'\x0981',L'\x0983',L'\x09bc',L'\x09bc',L'\x09be',L'\x09be',L'\x09bf',L'\x09bf',L'\x09c0',L'\x09c4',L'\x09c7',L'\x09c8',L'\x09cb',L'\x09cd',L'\x09d7',L'\x09d7',L'\x09e2',L'\x09e3',L'\x0a02',L'\x0a02',L'\x0a3c',L'\x0a3c',L'\x0a3e',L'\x0a3e',L'\x0a3f',L'\x0a3f',L'\x0a40',L'\x0a42',L'\x0a47',L'\x0a48',L'\x0a4b',L'\x0a4d',L'\x0a70',L'\x0a71',L'\x0a81',L'\x0a83'
,L'\x0abc',L'\x0abc',L'\x0abe',L'\x0ac5',L'\x0ac7',L'\x0ac9',L'\x0acb',L'\x0acd',L'\x0b01',L'\x0b03',L'\x0b3c',L'\x0b3c',L'\x0b3e',L'\x0b43',L'\x0b47',L'\x0b48',L'\x0b4b',L'\x0b4d',L'\x0b56',L'\x0b57',L'\x0b82',L'\x0b83',L'\x0bbe',L'\x0bc2',L'\x0bc6',L'\x0bc8',L'\x0bca',L'\x0bcd',L'\x0bd7',L'\x0bd7',L'\x0c01',L'\x0c03',L'\x0c3e',L'\x0c44',L'\x0c46',L'\x0c48',L'\x0c4a',L'\x0c4d',L'\x0c55',L'\x0c56'
,L'\x0c82',L'\x0c83',L'\x0cbe',L'\x0cc4',L'\x0cc6',L'\x0cc8',L'\x0cca',L'\x0ccd',L'\x0cd5',L'\x0cd6',L'\x0d02',L'\x0d03',L'\x0d3e',L'\x0d43',L'\x0d46',L'\x0d48',L'\x0d4a',L'\x0d4d',L'\x0d57',L'\x0d57',L'\x0e31',L'\x0e31',L'\x0e34',L'\x0e3a',L'\x0e47',L'\x0e4e',L'\x0eb1',L'\x0eb1',L'\x0eb4',L'\x0eb9',L'\x0ebb',L'\x0ebc',L'\x0ec8',L'\x0ecd',L'\x0f18',L'\x0f19',L'\x0f35',L'\x0f35',L'\x0f37',L'\x0f37'
,L'\x0f39',L'\x0f39',L'\x0f3e',L'\x0f3e',L'\x0f3f',L'\x0f3f',L'\x0f71',L'\x0f84',L'\x0f86',L'\x0f8b',L'\x0f90',L'\x0f95',L'\x0f97',L'\x0f97',L'\x0f99',L'\x0fad',L'\x0fb1',L'\x0fb7',L'\x0fb9',L'\x0fb9',L'\x20d0',L'\x20dc',L'\x20e1',L'\x20e1',L'\x302a',L'\x302f',L'\x3099',L'\x3099',L'\x309a',L'\x309a'
};

static const PRUint32 DIGIT_RANGE_LENGTH = 30;
static const PRUnichar DIGIT_RANGE[] = {
 L'\x0030',L'\x0039',L'\x0660',L'\x0669',L'\x06f0',L'\x06f9',L'\x0966',L'\x096f',L'\x09e6',L'\x09ef',L'\x0a66',L'\x0a6f',L'\x0ae6',L'\x0aef',L'\x0b66',L'\x0b6f',L'\x0be7',L'\x0bef',L'\x0c66',L'\x0c6f',L'\x0ce6',L'\x0cef',L'\x0d66',L'\x0d6f',L'\x0e50',L'\x0e59',L'\x0ed0',L'\x0ed9',L'\x0f20',L'\x0f29'
};

static const PRUint32 EXTENDER_RANGE_LENGTH = 22;
static const PRUnichar EXTENDER_RANGE[] = {
 L'\x00b7',L'\x00b7',L'\x02d0',L'\x02d0',L'\x02d1',L'\x02d1',L'\x0387',L'\x0387',L'\x0640',L'\x0640',L'\x0e46',L'\x0e46',L'\x0ec6',L'\x0ec6',L'\x3005',L'\x3005',L'\x3031',L'\x3035',L'\x309d',L'\x309e',L'\x30fc',L'\x30fe'
};

PRBool
nsXFormsXPathXMLUtil::IsInRange(const PRUnichar* range, const PRUint32 range_length, const PRUnichar c)
{
  PRBool inrange = false;
  for (PRUint32 i = 0; !inrange && c >= range[i] && i < range_length; i += 2) {
    inrange = c >= range[i] && c <= range[i + 1];
  }
  return inrange;
}

PRBool
nsXFormsXPathXMLUtil::IsWhitespace(const PRUnichar c)
{
  return (c == L'\r' || c == L' ' || c == L'\n' || c == L'\t');
}
    
PRBool
nsXFormsXPathXMLUtil::IsDigit(const PRUnichar c)
{
  return IsInRange(DIGIT_RANGE, DIGIT_RANGE_LENGTH, c);
}
    
PRBool
nsXFormsXPathXMLUtil::IsIdeographic(const PRUnichar c)
{
  return IsInRange(IDEOGRAPHIC_RANGE, IDEOGRAPHIC_RANGE_LENGTH, c);
}
    
PRBool
nsXFormsXPathXMLUtil::IsLetter(const PRUnichar c)
{
  return IsBaseChar(c) || IsIdeographic(c);
}

PRBool
nsXFormsXPathXMLUtil::IsBaseChar(const PRUnichar c)
{
  return IsInRange(BASECHAR_RANGE, BASECHAR_RANGE_LENGTH, c);
}
    
    
PRBool
nsXFormsXPathXMLUtil::IsNCNameChar(const PRUnichar c)
{
  return IsLetter(c) || IsDigit(c) || c == L'.' || c == L'-' || c == L'_' || IsCombiningChar(c) || IsExtender(c);
}
    
PRBool
nsXFormsXPathXMLUtil::IsCombiningChar(const PRUnichar c)
{
  return IsInRange(COMBINING_CHAR_RANGE, COMBINING_CHAR_RANGE_LENGTH, c);
}
    
PRBool
nsXFormsXPathXMLUtil::IsExtender(const PRUnichar c)
{
  return IsInRange(EXTENDER_RANGE, EXTENDER_RANGE_LENGTH, c);
} 
