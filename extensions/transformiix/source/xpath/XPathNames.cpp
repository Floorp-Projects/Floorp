/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *   -- added LANG_FN
 *
 */

/**
 * XPath names
**/

#include "FunctionLib.h"

//-- Function Names
const String XPathNames::BOOLEAN_FN              = "boolean";
const String XPathNames::CONCAT_FN               = "concat";
const String XPathNames::CONTAINS_FN             = "contains";
const String XPathNames::COUNT_FN                = "count";
const String XPathNames::FALSE_FN                = "false";
const String XPathNames::ID_FN                   = "id";
const String XPathNames::LAST_FN                 = "last";
const String XPathNames::LOCAL_NAME_FN           = "local-name";
const String XPathNames::NAME_FN                 = "name";
const String XPathNames::NAMESPACE_URI_FN        = "namespace-uri";
const String XPathNames::NORMALIZE_SPACE_FN      = "normalize-space";
const String XPathNames::NOT_FN                  = "not";
const String XPathNames::POSITION_FN             = "position";
const String XPathNames::STARTS_WITH_FN          = "starts-with";
const String XPathNames::STRING_FN               = "string";
const String XPathNames::STRING_LENGTH_FN        = "string-length";
const String XPathNames::SUBSTRING_FN            = "substring";
const String XPathNames::SUBSTRING_AFTER_FN      = "substring-after";
const String XPathNames::SUBSTRING_BEFORE_FN     = "substring-before";
const String XPathNames::SUM_FN                  = "sum";
const String XPathNames::TRANSLATE_FN            = "translate";
const String XPathNames::TRUE_FN                 = "true";
// OG+
const String XPathNames::NUMBER_FN               = "number";
const String XPathNames::ROUND_FN                = "round";
const String XPathNames::CEILING_FN              = "ceiling";
const String XPathNames::FLOOR_FN                = "floor";
// OG-
//Marina M. boolean function lang
const String XPathNames::LANG_FN                 = "lang";

//-- internal XSL processor functions
const String XPathNames::ERROR_FN                = "error";

