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

// define the bits used to handle the operator

#define NS_MATHML_OPERATOR_MUTABLE     0x80000000 // the very first bit
#define NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR     0x40000000 // the second bit

// define the bits used in the Operator Dictionary
// XXX replace with the PR_BIT(n) macro

#define NS_MATHML_OPERATOR_FORM  0x3 // the very last two bits tell us the form
#define     NS_MATHML_OPERATOR_FORM_INFIX   1
#define     NS_MATHML_OPERATOR_FORM_PREFIX  2
#define     NS_MATHML_OPERATOR_FORM_POSTFIX 3
#define NS_MATHML_OPERATOR_STRETCHY      (1<<2)
#define NS_MATHML_OPERATOR_FENCE         (1<<3)
#define NS_MATHML_OPERATOR_ACCENT        (1<<4)
#define NS_MATHML_OPERATOR_LARGEOP       (1<<5)
#define NS_MATHML_OPERATOR_SEPARATOR     (1<<6)
#define NS_MATHML_OPERATOR_MOVABLELIMITS (1<<7)

// Additional bits not stored in the dictionary

#define NS_MATHML_OPERATOR_SYMMETRIC     (1<<8)

#define NS_MATHML_OPERATOR_MINSIZE_EXPLICIT  (1<<9)
#define NS_MATHML_OPERATOR_MAXSIZE_EXPLICIT  (1<<10)

// Macros that retrieve those bits

#define NS_MATHML_OPERATOR_IS_MUTABLE(_flags) \
  (NS_MATHML_OPERATOR_MUTABLE == ((_flags) & NS_MATHML_OPERATOR_MUTABLE))

#define NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(_flags) \
  (NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR == ((_flags) & NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR))

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

#define NS_MATHML_OPERATOR_IS_SYMMETRIC(_flags) \
  (NS_MATHML_OPERATOR_SYMMETRIC == ((_flags) & NS_MATHML_OPERATOR_SYMMETRIC))

#define NS_MATHML_OPERATOR_MINSIZE_IS_EXPLICIT(_flags) \
  (NS_MATHML_OPERATOR_MINSIZE_EXPLICIT == ((_flags) & NS_MATHML_OPERATOR_MINSIZE_EXPLICIT))

#define NS_MATHML_OPERATOR_MAXSIZE_IS_EXPLICIT(_flags) \
  (NS_MATHML_OPERATOR_MAXSIZE_EXPLICIT == ((_flags) & NS_MATHML_OPERATOR_MAXSIZE_EXPLICIT))

class nsMathMLOperators {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // LookupOperator:
  // Given the string value of an operator and its form (last two bits of flags),
  // this method returns true if the operator is found in the Operator Dictionary.
  // Attributes of the operator are returned in the output parameters.
  // If the operator is not found under the supplied form but is found under a 
  // different form, the method returns true as well. The caller can test the
  // output parameter aFlags to know exactly under which form the operator was
  // found in the Operator Dictionary.
  static PRBool
  LookupOperator(const nsString&       aOperator,
                 const nsOperatorFlags aForm,
                 nsOperatorFlags*      aFlags,
                 float*                aLeftSpace,
                 float*                aRightSpace);
  static PRBool
  LookupOperator(const nsCString&      aOperator,
                 const nsOperatorFlags aForm,
                 nsOperatorFlags*      aFlags,
                 float*                aLeftSpace,
                 float*                aRightSpace);

  // MatchOperator:
  // Given the string value of an operator and *some* bits of its flags,
  // this function will return true if this operator matches with another
  // in the Operator Dictionary, i.e., if its string is the same as the 
  // other's string and its flags constitute a subset of the other's flags.
  // For example, to see if a string is a stretchy fence, the call will be:
  // MatchOperator(aString, NS_MATHML_OPERATOR_FENCE | NS_MATHML_OPERATOR_STRETCHY).
  static PRBool
  MatchOperator(const nsString&       aOperator,
                const nsOperatorFlags aFlags);
};

#endif /* nsMathMLOperators_h___ */
