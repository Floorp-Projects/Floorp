/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef lexutils_h___
#define lexutils_h___

#include <string>
#include <iterator>

#include "utilities.h"
#include "exception.h"
#include "vmtypes.h"
#include "jstypes.h"

namespace JavaScript {
namespace LexUtils {

#define IS_ALPHA(ch) ((ch >= 'a' && ch <= 'z') || \
                      (ch >= 'A' && ch <= 'Z') || \
                      (ch == '_'))
#define IS_NUMBER(ch) (ch >= '0' && ch <= '9')
    
    enum TokenEstimation {
        /* guess at tokentype, based on first character of token */
        teAlpha,
        teCloseParen,
        teComma,
        teColon,
        teEOF,
        teIllegal,
        teLessThan,
        teMinus,
        teNewline,
        teNumeric,
        teOpenParen,
        tePlus,
        teString,
        teUnknown,
    };

    struct TokenLocation {
        TokenLocation () : begin(0), estimate(teIllegal) {}
        string8_citer begin;
        TokenEstimation estimate;
    };
    
    /* compare, ignoring case */
    int cmp_nocase (const string8& s1, string8_citer s2_begin,
                    string8_citer s2_end);
    
    /* locate the beginning of the next token and take a guess at what it
     * might be */
    TokenLocation seekTokenStart (string8_citer begin, string8_citer end);

    /* general purpose lexer functions; |begin| is expected to point
     * at the start of the token to be processed (eg, these routines
     * don't call |seekTokenStart|, and no initial check is done to ensure
     * that |begin| != |end|.
     */
    string8_citer lexAlpha (string8_citer begin, string8_citer end,
                            string8 **rval);
    string8_citer lexBool (string8_citer begin, string8_citer end, bool *rval);
    string8_citer lexDouble (string8_citer begin, string8_citer end,
                             double *rval);
    string8_citer lexInt32 (string8_citer begin, string8_citer end,
                            int32 *rval);
    string8_citer lexRegister (string8_citer begin, string8_citer end,
                               JSTypes::Register *rval);
    string8_citer lexString8 (string8_citer begin, string8_citer end,
                              string8 **rval);
    string8_citer lexUInt32 (string8_citer begin, string8_citer end,
                             uint32 *rval);
    
}
}

#endif /* lexutils_h___ */
