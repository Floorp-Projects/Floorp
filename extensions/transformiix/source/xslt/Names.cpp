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
 * (C) 1999, 2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- Removed the trailing "s" from FOLLOWING_SIBLING_AXIS, and
 *       PRECEDING_SIBLING_AXIS to be compatible with the
 *       W3C XPath 1.0 Recommendation
 *    -- Added lang attr declaration
 *
 */

/**
 * XSL names used throughout the XSLProcessor.
 * Probably should be wrapped in a Namespace
**/
#include "Names.h"

//-- Global Strings
const String STYLESHEET_PI(NS_LITERAL_STRING("xml-stylesheet"));
const String STYLESHEET_PI_OLD(NS_LITERAL_STRING("xml:stylesheet"));
const String XSL_MIME_TYPE(NS_LITERAL_STRING("text/xsl"));

//-- Attribute Values
const String ASCENDING_VALUE(NS_LITERAL_STRING("ascending"));
const String DESCENDING_VALUE(NS_LITERAL_STRING("descending"));
const String LOWER_FIRST_VALUE(NS_LITERAL_STRING("lower-first"));
const String NUMBER_VALUE(NS_LITERAL_STRING("number"));
const String TEXT_VALUE(NS_LITERAL_STRING("text"));
const String UPPER_FIRST_VALUE(NS_LITERAL_STRING("upper-first"));
const String YES_VALUE(NS_LITERAL_STRING("yes"));

//-- Stylesheet attributes
const String ANCESTOR_AXIS(NS_LITERAL_STRING("ancestor"));
const String ANCESTOR_OR_SELF_AXIS(NS_LITERAL_STRING("ancestor-or-self"));
const String ATTRIBUTE_AXIS(NS_LITERAL_STRING("attribute"));
const String CHILD_AXIS(NS_LITERAL_STRING("child"));
const String DESCENDANT_AXIS(NS_LITERAL_STRING("descendant"));
const String DESCENDANT_OR_SELF_AXIS(NS_LITERAL_STRING("descendant-or-self"));
const String FOLLOWING_AXIS(NS_LITERAL_STRING("following"));
const String FOLLOWING_SIBLING_AXIS(NS_LITERAL_STRING("following-sibling"));
const String NAMESPACE_AXIS(NS_LITERAL_STRING("namespace"));
const String PARENT_AXIS(NS_LITERAL_STRING("parent"));
const String PRECEDING_AXIS(NS_LITERAL_STRING("preceding"));
const String PRECEDING_SIBLING_AXIS(NS_LITERAL_STRING("preceding-sibling"));
const String SELF_AXIS(NS_LITERAL_STRING("self"));
