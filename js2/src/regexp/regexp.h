/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * This struct holds a bitmap representation of a class from a regexp.
 * There's a list of these referenced by the classList field in the JSRegExp 
 * struct below. The initial state has startIndex set to the offset in the
 * original regexp source of the beginning of the class contents. The first 
 * use of the class converts the source representation into a bitmap.
 *
 */


typedef char16 jschar;
typedef uint8 jsbytecode;



#define JSREG_FOLD      0x01    /* fold uppercase to lowercase */
#define JSREG_GLOB      0x02    /* global exec, creates array of matches */
#define JSREG_MULTILINE 0x04    /* treat ^ and $ as begin and end of line */

typedef struct RECharSet RECharSet;

struct JS2RegExp {
    uint32       parenCount:24, /* number of parenthesized submatches */
                 flags:8;       /* flags, see above JSREG_* defines */
    uint32       classCount;    /* count [...] bitmaps */
    RECharSet    *classList;    /* list of [...] bitmaps */
    const jschar *source;       /* locked source string, sans // */
    jsbytecode   program[1];    /* regular expression bytecode */
};

typedef struct RECapture {
    int32 index;                /* start of contents, -1 for empty  */
    uint16 length;              /* length of capture */
} RECapture;

struct REMatchResult {
    uint32 startIndex;
    uint32 endIndex;
    uint32 parenCount;
    RECapture parens[1];      /* first of 'parenCount' captures, 
                               * allocated at end of this struct.
                               */
};

namespace JavaScript {      
namespace MetaData {

class JS2Metadata;

// Execute the re against the string starting at the index, return NULL for failure
REMatchResult *REExecute(JS2Metadata *meta, JS2RegExp *re, const jschar *str, uint32 index, uint32 length, bool globalMultiline);

// Compile the source re, return NULL for failure (error functions called)
JS2RegExp *RECompile(JS2Metadata *meta, const jschar *str, uint32 length, uint32 flags, bool flat);

// Compile the flag source and build a flag bit set. Return true/false for success/failure
bool parseFlags(JS2Metadata *meta, const jschar *flagStr, uint32 length, uint32 *flags);

// Execute the re against the string, but don't try advancing into the string
REMatchResult *REMatch(JS2Metadata *meta, JS2RegExp *re, const jschar *str, uint32 length);

void js_DestroyRegExp(JS2RegExp *re);

}
}
