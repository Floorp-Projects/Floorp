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
 *    -- original author.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- Removed the trailing "s" from FOLLOWING_SIBLING_AXIS, and
 *       PRECEDING_SIBLING_AXIS to be compatible with the
 *       W3C XPath 1.0 Recommendation
 *    -- Added lang attr declaration
 *
 */

#ifndef TRANSFRMX_NAMES_H
#define TRANSFRMX_NAMES_H

#include "TxString.h"

//-- Global Strings
extern const String STYLESHEET_PI;
extern const String STYLESHEET_PI_OLD;
extern const String XSL_MIME_TYPE;

//-- Attribute Values
extern const String ASCENDING_VALUE;
extern const String DESCENDING_VALUE;
extern const String LOWER_FIRST_VALUE;
extern const String MULTIPLE_VALUE;
extern const String NUMBER_VALUE;
extern const String PRESERVE_VALUE;
extern const String TEXT_VALUE;
extern const String UPPER_FIRST_VALUE;
extern const String YES_VALUE;

//-- Stylesheet attributes
extern const String ANCESTOR_AXIS;
extern const String ANCESTOR_OR_SELF_AXIS;
extern const String ATTRIBUTE_AXIS;
extern const String CHILD_AXIS;
extern const String DESCENDANT_AXIS;
extern const String DESCENDANT_OR_SELF_AXIS;
extern const String FOLLOWING_AXIS;
extern const String FOLLOWING_SIBLING_AXIS;
extern const String NAMESPACE_AXIS;
extern const String PARENT_AXIS;
extern const String PRECEDING_AXIS;
extern const String PRECEDING_SIBLING_AXIS;
extern const String SELF_AXIS;

#endif
