/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLOperators_h___
#define nsMathMLOperators_h___

#include <stdint.h>
#include "nsStringFwd.h"

enum nsStretchDirection {
  NS_STRETCH_DIRECTION_UNSUPPORTED = -1,
  NS_STRETCH_DIRECTION_DEFAULT = 0,
  NS_STRETCH_DIRECTION_HORIZONTAL = 1,
  NS_STRETCH_DIRECTION_VERTICAL = 2
};

typedef uint32_t nsOperatorFlags;
enum {
  // define the bits used to handle the operator
  NS_MATHML_OPERATOR_MUTABLE = 1 << 30,
  NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR = 1 << 29,
  NS_MATHML_OPERATOR_EMBELLISH_ISOLATED = 1 << 28,
  NS_MATHML_OPERATOR_CENTERED = 1 << 27,
  NS_MATHML_OPERATOR_INVISIBLE = 1 << 26,

  // define the bits used in the Operator Dictionary

  // the very last two bits tell us the form
  NS_MATHML_OPERATOR_FORM = 0x3,
  NS_MATHML_OPERATOR_FORM_INFIX = 1,
  NS_MATHML_OPERATOR_FORM_PREFIX = 2,
  NS_MATHML_OPERATOR_FORM_POSTFIX = 3,

  // the next 2 bits tell us the direction
  NS_MATHML_OPERATOR_DIRECTION = 0x3 << 2,
  NS_MATHML_OPERATOR_DIRECTION_HORIZONTAL = 1 << 2,
  NS_MATHML_OPERATOR_DIRECTION_VERTICAL = 2 << 2,

  // other bits used in the Operator Dictionary
  NS_MATHML_OPERATOR_STRETCHY = 1 << 4,
  NS_MATHML_OPERATOR_FENCE = 1 << 5,
  NS_MATHML_OPERATOR_ACCENT = 1 << 6,
  NS_MATHML_OPERATOR_LARGEOP = 1 << 7,
  NS_MATHML_OPERATOR_SEPARATOR = 1 << 8,
  NS_MATHML_OPERATOR_MOVABLELIMITS = 1 << 9,
  NS_MATHML_OPERATOR_SYMMETRIC = 1 << 10,

  // Additional bits not stored in the dictionary
  NS_MATHML_OPERATOR_MINSIZE_ABSOLUTE = 1 << 11,
  NS_MATHML_OPERATOR_MAXSIZE_ABSOLUTE = 1 << 12,
  NS_MATHML_OPERATOR_LSPACE_ATTR = 1 << 13,
  NS_MATHML_OPERATOR_RSPACE_ATTR = 1 << 14
};

#define NS_MATHML_OPERATOR_SIZE_INFINITY (mozilla::PositiveInfinity<float>())

class nsMathMLOperators {
 public:
  static void AddRefTable(void);
  static void ReleaseTable(void);
  static void CleanUp();

  // LookupOperator:
  // Given the string value of an operator and its form (last two bits of
  // flags), this method returns attributes of the operator in the output
  // parameters. The return value indicates whether an entry was found.
  static bool LookupOperator(const nsString& aOperator, const uint8_t aForm,
                             nsOperatorFlags* aFlags, float* aLeadingSpace,
                             float* aTrailingSpace);

  // LookupOperatorWithFallback:
  // Same as LookupOperator but if the operator is not found under the supplied
  // form, then the other forms are tried in the following order: infix, postfix
  // prefix. The caller can test the output parameter aFlags to know exactly
  // under which form the operator was found in the Operator Dictionary.
  static bool LookupOperatorWithFallback(const nsString& aOperator,
                                         const uint8_t aForm,
                                         nsOperatorFlags* aFlags,
                                         float* aLeadingSpace,
                                         float* aTrailingSpace);

  // Helper functions used by the nsMathMLChar class.
  static bool IsMirrorableOperator(const nsString& aOperator);

  // Helper functions used by the nsMathMLChar class to determine whether
  // aOperator corresponds to an integral operator.
  static bool IsIntegralOperator(const nsString& aOperator);

  // Helper function used by the nsMathMLChar class.
  static nsStretchDirection GetStretchyDirection(const nsString& aOperator);
};

////////////////////////////////////////////////////////////////////////////
// Macros that retrieve the bits used to handle operators

#define NS_MATHML_OPERATOR_IS_MUTABLE(_flags) \
  (NS_MATHML_OPERATOR_MUTABLE == ((_flags) & NS_MATHML_OPERATOR_MUTABLE))

#define NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(_flags) \
  (NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR ==               \
   ((_flags) & NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR))

#define NS_MATHML_OPERATOR_EMBELLISH_IS_ISOLATED(_flags) \
  (NS_MATHML_OPERATOR_EMBELLISH_ISOLATED ==              \
   ((_flags) & NS_MATHML_OPERATOR_EMBELLISH_ISOLATED))

#define NS_MATHML_OPERATOR_IS_CENTERED(_flags) \
  (NS_MATHML_OPERATOR_CENTERED == ((_flags) & NS_MATHML_OPERATOR_CENTERED))

#define NS_MATHML_OPERATOR_IS_INVISIBLE(_flags) \
  (NS_MATHML_OPERATOR_INVISIBLE == ((_flags) & NS_MATHML_OPERATOR_INVISIBLE))

#define NS_MATHML_OPERATOR_GET_FORM(_flags) ((_flags) & NS_MATHML_OPERATOR_FORM)

#define NS_MATHML_OPERATOR_GET_DIRECTION(_flags) \
  ((_flags) & NS_MATHML_OPERATOR_DIRECTION)

#define NS_MATHML_OPERATOR_FORM_IS_INFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_INFIX == ((_flags) & NS_MATHML_OPERATOR_FORM))

#define NS_MATHML_OPERATOR_FORM_IS_PREFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_PREFIX == ((_flags) & NS_MATHML_OPERATOR_FORM))

#define NS_MATHML_OPERATOR_FORM_IS_POSTFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_POSTFIX == ((_flags) & NS_MATHML_OPERATOR_FORM))

#define NS_MATHML_OPERATOR_IS_DIRECTION_VERTICAL(_flags) \
  (NS_MATHML_OPERATOR_DIRECTION_VERTICAL ==              \
   ((_flags) & NS_MATHML_OPERATOR_DIRECTION))

#define NS_MATHML_OPERATOR_IS_DIRECTION_HORIZONTAL(_flags) \
  (NS_MATHML_OPERATOR_DIRECTION_HORIZONTAL ==              \
   ((_flags) & NS_MATHML_OPERATOR_DIRECTION))

#define NS_MATHML_OPERATOR_IS_STRETCHY(_flags) \
  (NS_MATHML_OPERATOR_STRETCHY == ((_flags) & NS_MATHML_OPERATOR_STRETCHY))

#define NS_MATHML_OPERATOR_IS_FENCE(_flags) \
  (NS_MATHML_OPERATOR_FENCE == ((_flags) & NS_MATHML_OPERATOR_FENCE))

#define NS_MATHML_OPERATOR_IS_ACCENT(_flags) \
  (NS_MATHML_OPERATOR_ACCENT == ((_flags) & NS_MATHML_OPERATOR_ACCENT))

#define NS_MATHML_OPERATOR_IS_LARGEOP(_flags) \
  (NS_MATHML_OPERATOR_LARGEOP == ((_flags) & NS_MATHML_OPERATOR_LARGEOP))

#define NS_MATHML_OPERATOR_IS_SEPARATOR(_flags) \
  (NS_MATHML_OPERATOR_SEPARATOR == ((_flags) & NS_MATHML_OPERATOR_SEPARATOR))

#define NS_MATHML_OPERATOR_IS_MOVABLELIMITS(_flags) \
  (NS_MATHML_OPERATOR_MOVABLELIMITS ==              \
   ((_flags) & NS_MATHML_OPERATOR_MOVABLELIMITS))

#define NS_MATHML_OPERATOR_IS_SYMMETRIC(_flags) \
  (NS_MATHML_OPERATOR_SYMMETRIC == ((_flags) & NS_MATHML_OPERATOR_SYMMETRIC))

#define NS_MATHML_OPERATOR_MINSIZE_IS_ABSOLUTE(_flags) \
  (NS_MATHML_OPERATOR_MINSIZE_ABSOLUTE ==              \
   ((_flags) & NS_MATHML_OPERATOR_MINSIZE_ABSOLUTE))

#define NS_MATHML_OPERATOR_MAXSIZE_IS_ABSOLUTE(_flags) \
  (NS_MATHML_OPERATOR_MAXSIZE_ABSOLUTE ==              \
   ((_flags) & NS_MATHML_OPERATOR_MAXSIZE_ABSOLUTE))

#define NS_MATHML_OPERATOR_HAS_LSPACE_ATTR(_flags) \
  (NS_MATHML_OPERATOR_LSPACE_ATTR ==               \
   ((_flags) & NS_MATHML_OPERATOR_LSPACE_ATTR))

#define NS_MATHML_OPERATOR_HAS_RSPACE_ATTR(_flags) \
  (NS_MATHML_OPERATOR_RSPACE_ATTR ==               \
   ((_flags) & NS_MATHML_OPERATOR_RSPACE_ATTR))

#endif /* nsMathMLOperators_h___ */
