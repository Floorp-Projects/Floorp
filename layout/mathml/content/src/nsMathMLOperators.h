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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#ifndef nsMathMLOperators_h___
#define nsMathMLOperators_h___

#include "nslayout.h"
#include "nsCoord.h"

typedef PRUint32 nsOperatorFlags;

typedef PRUint32 nsStretchDirection;

#define NS_STRETCH_DIRECTION_HORIZONTAL   0
#define NS_STRETCH_DIRECTION_VERTICAL     1

// Structure used for a char's size and alignment information.
struct nsCharMetrics {
//  nscoord leading;
  nscoord width, height;
  nscoord ascent, descent;
};

// define the bits used in the operator dictionary

#define NS_MATHML_OPERATOR_FORM  0x3 // the last two bits tell us the form
#define     NS_MATHML_OPERATOR_FORM_INFIX   1
#define     NS_MATHML_OPERATOR_FORM_PREFIX  2
#define     NS_MATHML_OPERATOR_FORM_POSTFIX 3
#define NS_MATHML_OPERATOR_STRETCHY      (1 << 2)
#define NS_MATHML_OPERATOR_FENCE         (1 << 3)
#define NS_MATHML_OPERATOR_ACCENT        (1 << 4)
#define NS_MATHML_OPERATOR_LARGEOP       (1 << 5)
#define NS_MATHML_OPERATOR_SEPARATOR     (1 << 6)
#define NS_MATHML_OPERATOR_MOVABLELIMITS (1 << 7)

// Macros that retrieve those bits

#define NS_MATHML_OPERATOR_GET_FORM(_flags) \
  ((_flags) & NS_MATHML_OPERATOR_FORM)

#define NS_MATHML_OPERATOR_FORM_IS_INFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_INFIX == ((_flags) & NS_MATHML_OPERATOR_FORM_INFIX))

#define NS_MATHML_OPERATOR_FORM_IS_PREFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_PREFIX == ((_flags) & NS_MATHML_OPERATOR_FORM_PREFIX))

#define NS_MATHML_OPERATOR_FORM_IS_POSTFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_POSTFIX == ((_flags) & NS_MATHML_OPERATOR_FORM_POSTFIX ))

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
  (NS_MATHML_OPERATOR_MOVABLELIMITS == ((_flags) & NS_MATHML_OPERATOR_MOVABLELIMITS))


class nsMathMLOperators {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given the string value of an operator and its form (last two bits of flags),
  // this method returns true if the operator is found in the operator dictionary.
  // Attributes of the operator are returned in the output parameters.

  static PRBool LookupOperator(const nsStr&          aOperator,
                               const nsOperatorFlags aForm,
                               nsOperatorFlags*      aFlags,
                               float*                aLeftSpace,
                               float*                aRightSpace);

};

#endif /* nsMathMLOperators_h___ */
