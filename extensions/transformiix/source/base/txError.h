/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
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

#ifndef __TX_ERROR
#define __TX_ERROR

/*
 * Error value mockup for standalone.
 * See nsError.h for details.
 */

#include "nsError.h"

#define NS_ERROR_XPATH_INVALID_ARG         NS_ERROR_INVALID_ARG

#define NS_XSLT_GET_NEW_HANDLER                        \
    NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_XSLT, 1)

#define NS_ERROR_XSLT_PARSE_FAILURE                    \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 1)

#define NS_ERROR_XPATH_PARSE_FAILURE                   \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 2)

#define NS_ERROR_XSLT_ALREADY_SET                      \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 3)

#define NS_ERROR_XSLT_EXECUTION_FAILURE                \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 4)

#define NS_ERROR_XPATH_UNKNOWN_FUNCTION                \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 5)

#define NS_ERROR_XSLT_BAD_RECURSION                    \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 6)

#define NS_ERROR_XSLT_BAD_VALUE                        \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 7)

#define NS_ERROR_XSLT_NODESET_EXPECTED                 \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 8)

#define NS_ERROR_XSLT_ABORTED                          \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 9)

#define NS_ERROR_XSLT_NETWORK_ERROR                    \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 10)

#define NS_ERROR_XSLT_WRONG_MIME_TYPE                  \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 11)

#define NS_ERROR_XSLT_LOAD_RECURSION                   \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 12)

#define NS_ERROR_XPATH_BAD_ARGUMENT_COUNT              \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 13)

#define NS_ERROR_XPATH_BAD_EXTENSION_FUNCTION          \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 14)

#define NS_ERROR_XPATH_PAREN_EXPECTED                  \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 15)

#define NS_ERROR_XPATH_INVALID_AXIS                    \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 16)

#define NS_ERROR_XPATH_NO_NODE_TYPE_TEST               \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 17)

#define NS_ERROR_XPATH_BRACKET_EXPECTED                \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 18)

#define NS_ERROR_XPATH_INVALID_VAR_NAME                \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 19)

#define NS_ERROR_XPATH_UNEXPECTED_END                  \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 20)

#define NS_ERROR_XPATH_OPERATOR_EXPECTED               \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 21)

#define NS_ERROR_XPATH_UNCLOSED_LITERAL                \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 22)

#define NS_ERROR_XPATH_BAD_COLON                       \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 23)

#define NS_ERROR_XPATH_BAD_BANG                        \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 24)

#define NS_ERROR_XPATH_ILLEGAL_CHAR                    \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 25)

#define NS_ERROR_XPATH_BINARY_EXPECTED                 \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 26)

#define NS_ERROR_XSLT_LOAD_BLOCKED_ERROR               \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 27)

#define NS_ERROR_XPATH_INVALID_EXPRESSION_EVALUATED    \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XSLT, 28)

#endif // __TX_ERROR
