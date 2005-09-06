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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsMathMLOperators_h___
#define nsMathMLOperators_h___

#include "nsCoord.h"

typedef PRUint32 nsOperatorFlags;
typedef PRInt32 nsStretchDirection;

#define NS_STRETCH_DIRECTION_UNSUPPORTED  -1
#define NS_STRETCH_DIRECTION_DEFAULT       0
#define NS_STRETCH_DIRECTION_HORIZONTAL    1
#define NS_STRETCH_DIRECTION_VERTICAL      2

// define the bits used to handle the operator

#define NS_MATHML_OPERATOR_MUTABLE            (1<<31)
#define NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR (1<<30)
#define NS_MATHML_OPERATOR_EMBELLISH_ISOLATED (1<<29)
#define NS_MATHML_OPERATOR_CENTERED           (1<<28)
#define NS_MATHML_OPERATOR_INVISIBLE          (1<<27)

// define the bits used in the Operator Dictionary

#define NS_MATHML_OPERATOR_FORM  0x3 // the very last two bits tell us the form
#define     NS_MATHML_OPERATOR_FORM_INFIX      1
#define     NS_MATHML_OPERATOR_FORM_PREFIX     2
#define     NS_MATHML_OPERATOR_FORM_POSTFIX    3
#define NS_MATHML_OPERATOR_STRETCHY 0xC // the 2 penultimate last bits are used
#define     NS_MATHML_OPERATOR_STRETCHY_VERT  (1<<2)
#define     NS_MATHML_OPERATOR_STRETCHY_HORIZ (1<<3)
#define NS_MATHML_OPERATOR_FENCE              (1<<4)
#define NS_MATHML_OPERATOR_ACCENT             (1<<5)
#define NS_MATHML_OPERATOR_LARGEOP            (1<<6)
#define NS_MATHML_OPERATOR_SEPARATOR          (1<<7)
#define NS_MATHML_OPERATOR_MOVABLELIMITS      (1<<8)
#define NS_MATHML_OPERATOR_SYMMETRIC          (1<<9)

// Additional bits not stored in the dictionary

#define NS_MATHML_OPERATOR_MINSIZE_EXPLICIT   (1<<10)
#define NS_MATHML_OPERATOR_MAXSIZE_EXPLICIT   (1<<11)
#define NS_MATHML_OPERATOR_LEFTSPACE_ATTR     (1<<12)
#define NS_MATHML_OPERATOR_RIGHTSPACE_ATTR    (1<<13)


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

   // LookupOperators:
   // Helper to return all the forms under which an operator is listed in the
   // Operator Dictionary. The caller must pass arrays of size 4, and use 
   // aFlags[NS_MATHML_OPERATOR_FORM_{INFIX|POSTFIX|PREFIX}], aLeftSpace[], etc,
   // to access the attributes of the operator under a particular form. If the
   // operator wasn't found under a form, its entry aFlags[form] is set to zero.
   static void
   LookupOperators(const nsString&       aOperator,
                   nsOperatorFlags*      aFlags,
                   float*                aLeftSpace,
                   float*                aRightSpace);

  // IsMutableOperator:
  // Return true if the operator exists and is stretchy or largeop
  static PRBool
  IsMutableOperator(const nsString& aOperator);

  // Helper functions for stretchy operators. These are used by the
  // nsMathMLChar class.
  static PRInt32 CountStretchyOperator();
  static PRInt32 FindStretchyOperator(PRUnichar aOperator);
  static nsStretchDirection GetStretchyDirectionAt(PRInt32 aIndex);
  static void DisableStretchyOperatorAt(PRInt32 aIndex);


  // Style invariant chararacters (chars have their own intrinsic predefined style)
  enum eMATHVARIANT {
    eMATHVARIANT_NONE = -1,
    eMATHVARIANT_normal = 0,
    eMATHVARIANT_bold,
    eMATHVARIANT_italic,
    eMATHVARIANT_bold_italic,
    eMATHVARIANT_sans_serif,
    eMATHVARIANT_bold_sans_serif,
    eMATHVARIANT_sans_serif_italic,
    eMATHVARIANT_sans_serif_bold_italic,
    eMATHVARIANT_monospace,
    eMATHVARIANT_script,
    eMATHVARIANT_bold_script,
    eMATHVARIANT_fraktur,
    eMATHVARIANT_bold_fraktur,
    eMATHVARIANT_double_struck,
    eMATHVARIANT_COUNT
  };
  static PRBool LookupInvariantChar(PRUnichar     aChar,
                                    eMATHVARIANT* aType = nsnull);
};


////////////////////////////////////////////////////////////////////////////
// Macros that retrieve the bits used to handle operators

#define NS_MATHML_OPERATOR_IS_MUTABLE(_flags) \
  (NS_MATHML_OPERATOR_MUTABLE == ((_flags) & NS_MATHML_OPERATOR_MUTABLE))

#define NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(_flags) \
  (NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR == ((_flags) & NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR))

#define NS_MATHML_OPERATOR_EMBELLISH_IS_ISOLATED(_flags) \
  (NS_MATHML_OPERATOR_EMBELLISH_ISOLATED == ((_flags) & NS_MATHML_OPERATOR_EMBELLISH_ISOLATED))

#define NS_MATHML_OPERATOR_IS_CENTERED(_flags) \
  (NS_MATHML_OPERATOR_CENTERED == ((_flags) & NS_MATHML_OPERATOR_CENTERED))

#define NS_MATHML_OPERATOR_IS_INVISIBLE(_flags) \
  (NS_MATHML_OPERATOR_INVISIBLE == ((_flags) & NS_MATHML_OPERATOR_INVISIBLE))

#define NS_MATHML_OPERATOR_GET_FORM(_flags) \
  ((_flags) & NS_MATHML_OPERATOR_FORM)

#define NS_MATHML_OPERATOR_GET_STRETCHY_DIR(_flags) \
  ((_flags) & NS_MATHML_OPERATOR_STRETCHY)

#define NS_MATHML_OPERATOR_FORM_IS_INFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_INFIX == ((_flags) & NS_MATHML_OPERATOR_FORM_INFIX))

#define NS_MATHML_OPERATOR_FORM_IS_PREFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_PREFIX == ((_flags) & NS_MATHML_OPERATOR_FORM_PREFIX))

#define NS_MATHML_OPERATOR_FORM_IS_POSTFIX(_flags) \
  (NS_MATHML_OPERATOR_FORM_POSTFIX == ((_flags) & NS_MATHML_OPERATOR_FORM_POSTFIX ))

#define NS_MATHML_OPERATOR_IS_STRETCHY(_flags) \
  (0 != ((_flags) & NS_MATHML_OPERATOR_STRETCHY))

#define NS_MATHML_OPERATOR_IS_STRETCHY_VERT(_flags) \
  (NS_MATHML_OPERATOR_STRETCHY_VERT == ((_flags) & NS_MATHML_OPERATOR_STRETCHY_VERT))

#define NS_MATHML_OPERATOR_IS_STRETCHY_HORIZ(_flags) \
  (NS_MATHML_OPERATOR_STRETCHY_HORIZ == ((_flags) & NS_MATHML_OPERATOR_STRETCHY_HORIZ))

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

#define NS_MATHML_OPERATOR_HAS_LEFTSPACE_ATTR(_flags) \
  (NS_MATHML_OPERATOR_LEFTSPACE_ATTR == ((_flags) & NS_MATHML_OPERATOR_LEFTSPACE_ATTR))

#define NS_MATHML_OPERATOR_HAS_RIGHTSPACE_ATTR(_flags) \
  (NS_MATHML_OPERATOR_RIGHTSPACE_ATTR == ((_flags) & NS_MATHML_OPERATOR_RIGHTSPACE_ATTR))

#endif /* nsMathMLOperators_h___ */
