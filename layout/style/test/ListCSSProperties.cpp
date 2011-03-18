/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is ListCSSProperties.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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

/* build (from code) lists of all supported CSS properties */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct PropertyInfo {
    const char *propName;
    const char *domName;
};

const PropertyInfo gLonghandProperties[] = {

#define CSS_PROP_DOMPROP_PREFIXED(prop_) Moz ## prop_
#define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_,            \
                 parsevariant_, kwtable_, stylestruct_, stylestructoffset_,    \
                 animtype_)                                                    \
    { #name_, #method_ },

#include "nsCSSPropList.h"

#undef CSS_PROP
#undef CSS_PROP_DOMPROP_PREFIXED

};

/*
 * These are the properties for which domName in the above list should
 * be used.  They're in the same order as the above list, with some
 * items skipped.
 */
const char* gLonghandPropertiesWithDOMProp[] = {

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_,            \
                 parsevariant_, kwtable_, stylestruct_, stylestructoffset_,    \
                 animtype_)                                                    \
    #name_,

#include "nsCSSPropList.h"

#undef CSS_PROP
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL

};

const PropertyInfo gShorthandProperties[] = {

#define CSS_PROP_DOMPROP_PREFIXED(prop_) Moz ## prop_
// Need an extra level of macro nesting to force expansion of method_
// params before they get pasted.
#define LISTCSSPROPERTIES_INNER_MACRO(method_) #method_,
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) \
    { #name_, LISTCSSPROPERTIES_INNER_MACRO(method_) },

#include "nsCSSPropList.h"

#undef CSS_PROP_SHORTHAND
#undef LISTCSSPROPERTIES_INNER_MACRO
#undef CSS_PROP_DOMPROP_PREFIXED

};

/* see gLonghandPropertiesWithDOMProp */
const char* gShorthandPropertiesWithDOMProp[] = {

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) \
    #name_,

#include "nsCSSPropList.h"

#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL

};


#define ARRAY_LENGTH(a_) (sizeof(a_)/sizeof((a_)[0]))

const char *gInaccessibleProperties[] = {
    // Don't print the properties that aren't accepted by the parser, per
    // CSSParserImpl::ParseProperty
    "-x-system-font",
    "border-end-color-value",
    "border-end-style-value",
    "border-end-width-value",
    "border-left-color-value",
    "border-left-color-ltr-source",
    "border-left-color-rtl-source",
    "border-left-style-value",
    "border-left-style-ltr-source",
    "border-left-style-rtl-source",
    "border-left-width-value",
    "border-left-width-ltr-source",
    "border-left-width-rtl-source",
    "border-right-color-value",
    "border-right-color-ltr-source",
    "border-right-color-rtl-source",
    "border-right-style-value",
    "border-right-style-ltr-source",
    "border-right-style-rtl-source",
    "border-right-width-value",
    "border-right-width-ltr-source",
    "border-right-width-rtl-source",
    "border-start-color-value",
    "border-start-style-value",
    "border-start-width-value",
    "margin-end-value",
    "margin-left-value",
    "margin-right-value",
    "margin-start-value",
    "margin-left-ltr-source",
    "margin-left-rtl-source",
    "margin-right-ltr-source",
    "margin-right-rtl-source",
    "padding-end-value",
    "padding-left-value",
    "padding-right-value",
    "padding-start-value",
    "padding-left-ltr-source",
    "padding-left-rtl-source",
    "padding-right-ltr-source",
    "padding-right-rtl-source",
    "-moz-script-level", // parsed by UA sheets only
    "-moz-script-size-multiplier",
    "-moz-script-min-size"
};

inline int
is_inaccessible(const char* aPropName)
{
    for (unsigned j = 0; j < ARRAY_LENGTH(gInaccessibleProperties); ++j) {
        if (strcmp(aPropName, gInaccessibleProperties[j]) == 0)
            return 1;
    }
    return 0;
}

void
print_array(const char *aName,
            const PropertyInfo *aProps, unsigned aPropsLength,
            const char * const * aDOMProps, unsigned aDOMPropsLength)
{
    printf("var %s = [\n", aName);

    int first = 1;
    unsigned j = 0; // index into DOM prop list
    for (unsigned i = 0; i < aPropsLength; ++i) {
        const PropertyInfo *p = aProps + i;

        if (is_inaccessible(p->propName))
            // inaccessible properties never have DOM props, so don't
            // worry about incrementing j.  The assertion below will
            // catch if they do.
            continue;

        if (first)
            first = 0;
        else
            printf(",\n");

        printf("\t{ name: \"%s\", prop: ", p->propName);
        if (j >= aDOMPropsLength || strcmp(p->propName, aDOMProps[j]) != 0)
            printf("null");
        else {
            ++j;
            if (strncmp(p->domName, "Moz", 3) == 0)
                printf("\"%s\"", p->domName);
            else
                // lowercase the first letter
                printf("\"%c%s\"", p->domName[0] + 32, p->domName + 1);
        }
        printf("}");
    }

    if (j != aDOMPropsLength) {
        fprintf(stderr, "Assertion failure %s:%d\n", __FILE__, __LINE__);
        fprintf(stderr, "j==%d, aDOMPropsLength == %d\n", j, aDOMPropsLength);
        exit(1);
    }

    printf("\n];\n\n");
}

int
main()
{
    print_array("gLonghandProperties",
                gLonghandProperties,
                ARRAY_LENGTH(gLonghandProperties),
                gLonghandPropertiesWithDOMProp,
                ARRAY_LENGTH(gLonghandPropertiesWithDOMProp));
    print_array("gShorthandProperties",
                gShorthandProperties,
                ARRAY_LENGTH(gShorthandProperties),
                gShorthandPropertiesWithDOMProp,
                ARRAY_LENGTH(gShorthandPropertiesWithDOMProp));
    return 0;
}
