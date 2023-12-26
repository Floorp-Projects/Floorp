/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* build (from code) lists of all supported CSS properties */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mozilla/ArrayUtils.h"

// Do not consider properties not valid in style rules
#define CSS_PROP_LIST_EXCLUDE_NOT_IN_STYLE

// Need an extra level of macro nesting to force expansion of method_
// params before they get pasted.
#define STRINGIFY_METHOD(method_) #method_

struct PropertyInfo {
  const char* propName;
  const char* domName;
  const char* pref;
};

const PropertyInfo gLonghandProperties[] = {

#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) publicname_
#define CSS_PROP_LONGHAND(name_, id_, method_, flags_, pref_, ...) \
  {#name_, STRINGIFY_METHOD(method_), pref_},

#include "mozilla/ServoCSSPropList.h"

#undef CSS_PROP_LONGHAND
#undef CSS_PROP_PUBLIC_OR_PRIVATE

};

/*
 * These are the properties for which domName in the above list should
 * be used.  They're in the same order as the above list, with some
 * items skipped.
 */
const char* gLonghandPropertiesWithDOMProp[] = {

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_LONGHAND(name_, ...) #name_,

#include "mozilla/ServoCSSPropList.h"

#undef CSS_PROP_LONGHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL

};

const PropertyInfo gShorthandProperties[] = {

#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) publicname_
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) \
  {#name_, STRINGIFY_METHOD(method_), pref_},
#define CSS_PROP_ALIAS(name_, aliasid_, id_, method_, flags_, pref_) \
  {#name_, #method_, pref_},

#include "mozilla/ServoCSSPropList.h"

#undef CSS_PROP_ALIAS
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_PUBLIC_OR_PRIVATE

};

/* see gLonghandPropertiesWithDOMProp */
const char* gShorthandPropertiesWithDOMProp[] = {

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) #name_,
#define CSS_PROP_ALIAS(name_, aliasid_, id_, method_, flags_, pref_) #name_,

#include "mozilla/ServoCSSPropList.h"

#undef CSS_PROP_ALIAS
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL

};

#undef STRINGIFY_METHOD

const char* gInaccessibleProperties[] = {
    // Don't print the properties that aren't accepted by the parser, per
    // CSSParserImpl::ParseProperty
    "-x-cols",
    "-x-lang",
    "-x-span",
    "-x-text-scale",
    "-moz-default-appearance",
    "-moz-theme",
    "-moz-inert",
    "-moz-script-level",  // parsed by UA sheets only
    "-moz-math-variant",
    "-moz-math-display",                  // parsed by UA sheets only
    "-moz-top-layer",                     // parsed by UA sheets only
    "-moz-min-font-size-ratio",           // parsed by UA sheets only
    "-moz-box-collapse",                  // chrome-only internal properties
    "-moz-subtree-hidden-only-visually",  // chrome-only internal properties
    "-moz-user-focus",                    // chrome-only internal properties
    "-moz-window-input-region-margin",    // chrome-only internal properties
    "-moz-window-opacity",                // chrome-only internal properties
    "-moz-window-transform",              // chrome-only internal properties
    "-moz-window-transform-origin",       // chrome-only internal properties
    "-moz-window-shadow",                 // chrome-only internal properties
};

inline int is_inaccessible(const char* aPropName) {
  for (unsigned j = 0; j < MOZ_ARRAY_LENGTH(gInaccessibleProperties); ++j) {
    if (strcmp(aPropName, gInaccessibleProperties[j]) == 0) return 1;
  }
  return 0;
}

void print_array(const char* aName, const PropertyInfo* aProps,
                 unsigned aPropsLength, const char* const* aDOMProps,
                 unsigned aDOMPropsLength) {
  printf("var %s = [\n", aName);

  int first = 1;
  unsigned j = 0;  // index into DOM prop list
  for (unsigned i = 0; i < aPropsLength; ++i) {
    const PropertyInfo* p = aProps + i;

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
    if (p->pref[0]) {
      printf(", pref: \"%s\"", p->pref);
    }
    printf(" }");
  }

  if (j != aDOMPropsLength) {
    fprintf(stderr, "Assertion failure %s:%d\n", __FILE__, __LINE__);
    fprintf(stderr, "j==%d, aDOMPropsLength == %d\n", j, aDOMPropsLength);
    exit(1);
  }

  printf("\n];\n\n");
}

int main() {
  print_array("gLonghandProperties", gLonghandProperties,
              MOZ_ARRAY_LENGTH(gLonghandProperties),
              gLonghandPropertiesWithDOMProp,
              MOZ_ARRAY_LENGTH(gLonghandPropertiesWithDOMProp));
  print_array("gShorthandProperties", gShorthandProperties,
              MOZ_ARRAY_LENGTH(gShorthandProperties),
              gShorthandPropertiesWithDOMProp,
              MOZ_ARRAY_LENGTH(gShorthandPropertiesWithDOMProp));
  return 0;
}
