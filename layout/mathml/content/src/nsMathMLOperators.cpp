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

#include "nsString.h"
#include "nsAVLTree.h"

#include "nsMathMLOperators.h"


// get the actual value generated for NS_MATHML_OPERATOR_COUNT
#define WANT_MATHML_OPERATOR_COUNT
#include "nsMathMLOperatorList.h"
#undef WANT_MATHML_OPERATOR_COUNT

#include "nslog.h"

NS_IMPL_LOG(nsMathMLOperatorsLog)
#define PRINTF NS_LOG_PRINTF(nsMathMLOperatorsLog)
#define FLUSH  NS_LOG_FLUSH(nsMathMLOperatorsLog)

// define a zero-separated linear array of all MathML Operators in Unicode
const PRUnichar kMathMLOperator[] = {
#define MATHML_OPERATOR(_i,_operator,_flags,_lspace,_rspace) _operator, 0x0000,
#include "nsMathMLOperatorList.h"
#undef MATHML_OPERATOR
};


// operator dictionary entry
struct OperatorNode {
  OperatorNode(void)
    : mStr(),
      mFlags(0),
      mLeftSpace(0.0f),
      mRightSpace(0.0f)
  {
    nsStr::Initialize(mStr, eTwoByte); // with MathML, we are two-byte by default
  }

  OperatorNode(const nsStr& aStringValue, const nsOperatorFlags aFlags)
    : mStr(),
      mFlags(aFlags),
      mLeftSpace(0.0f),
      mRightSpace(0.0f)
  { // point to the incomming buffer
    // note that the incomming buffer may really be 2 byte
    nsStr::Initialize(mStr, aStringValue.mStr, aStringValue.mCapacity,
                      aStringValue.mLength, aStringValue.mCharSize, PR_FALSE);
  }
  
  void
  Init(const nsOperatorFlags aFlags, 
       const float           aLeftSpace, 
       const float           aRightSpace)
  {
    mFlags      = aFlags;
    mLeftSpace  = aLeftSpace;
    mRightSpace = aRightSpace;
  };

  // member data
  nsString        mStr;
  nsOperatorFlags mFlags;
  float           mLeftSpace;   // unit is em
  float           mRightSpace;  // unit is em
};

/*
  The MathML REC says: 
  "If the operator does not occur in the dictionary with the specified form,
  the renderer should use one of the forms which is available there, in the 
  order of preference: infix, postfix, prefix."
  
  The following variable will be used to keep track of all possible forms
  encountered in the Operator Dictionary. 
*/
static OperatorNode* gOperatorFound[4];

// index comparitor: based on the string of the operator and its form bits
class OperatorComparitor: public nsAVLNodeComparitor {
public:
  virtual ~OperatorComparitor(void) {}
  virtual PRInt32 operator()(void* aItem1, void* aItem2) {
    OperatorNode* one = (OperatorNode*)aItem1;
    OperatorNode* two = (OperatorNode*)aItem2;

    PRInt32 rv;
    rv = one->mStr.CompareWithConversion(two->mStr, PR_FALSE);
    if (0 == rv) {
      nsOperatorFlags form1 = NS_MATHML_OPERATOR_GET_FORM(one->mFlags);
      nsOperatorFlags form2 = NS_MATHML_OPERATOR_GET_FORM(two->mFlags);
      if (form1 == form2) return 0;
      else {
        rv = (form1 < form2) ? -1 : 1;

        // Record that the operator was found in a different form.
        // We don't know which is the input, we grab both of them and 
        // we check later (see below).
        gOperatorFound[form1] = one;
        gOperatorFound[form2] = two;
      }
    }
    return rv;
  }
};



static PRInt32             gTableRefCount = 0;
static OperatorNode*       gOperatorArray;
static nsAVLTree*          gOperatorTree;
static OperatorComparitor* gComparitor;


void
nsMathMLOperators::AddRefTable(void)
{
  if (0 == gTableRefCount++) {
    if (!gOperatorArray) {
      gOperatorArray = new OperatorNode[NS_MATHML_OPERATOR_COUNT];
      gComparitor = new OperatorComparitor();
      if (gComparitor) {
        gOperatorTree = new nsAVLTree(*gComparitor, nsnull);
      }
      if (gOperatorArray && gOperatorTree) {
#define MATHML_OPERATOR(_i,_operator,_flags,_lspace,_rspace) \
gOperatorArray[_i].Init(nsOperatorFlags(_flags),float(_lspace),float(_rspace));
#include "nsMathMLOperatorList.h"
#undef MATHML_OPERATOR
        const PRUnichar* cp = &kMathMLOperator[0];
        PRInt32 i = -1;
        while (++i < PRInt32(NS_MATHML_OPERATOR_COUNT)) {
          gOperatorArray[i].mStr = *cp++;
          while (*cp) { gOperatorArray[i].mStr.Append(*cp++); }
          cp++; // skip null separator
          gOperatorTree->AddItem(&(gOperatorArray[i]));
        }
      }
    }
  }
}

void
nsMathMLOperators::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    if (gOperatorArray) {
      delete[] gOperatorArray;
      gOperatorArray = nsnull;
    }
    if (gOperatorTree) {
      delete gOperatorTree;
      gOperatorTree = nsnull;
    }
    if (gComparitor) {
      delete gComparitor;
      gComparitor = nsnull;
    }
  }
}


PRBool
nsMathMLOperators::LookupOperator(const nsString&       aOperator, 
                                  const nsOperatorFlags aForm,
                                  nsOperatorFlags*      aFlags,
                                  float*                aLeftSpace,
                                  float*                aRightSpace)
{
  NS_ASSERTION(aFlags && aLeftSpace && aRightSpace, "bad usage");
  NS_ASSERTION(gOperatorTree, "no lookup table, needs addref");
  if (gOperatorTree) {

    gOperatorFound[NS_MATHML_OPERATOR_FORM_INFIX] = nsnull;
    gOperatorFound[NS_MATHML_OPERATOR_FORM_POSTFIX] = nsnull;
    gOperatorFound[NS_MATHML_OPERATOR_FORM_PREFIX] = nsnull;

    OperatorNode node(aOperator, aForm);
    OperatorNode* found = (OperatorNode*)gOperatorTree->FindItem(&node);

    // Check if the operator was perhaps found in a different form.
    // This is where we check that we are not referring to the input itself.
    if (!found) found = gOperatorFound[NS_MATHML_OPERATOR_FORM_INFIX];
    if (found == &node) found = nsnull;
    if (!found) found = gOperatorFound[NS_MATHML_OPERATOR_FORM_POSTFIX];
    if (found == &node) found = nsnull;
    if (!found) found = gOperatorFound[NS_MATHML_OPERATOR_FORM_PREFIX]; 
    if (found == &node) found = nsnull;

    if (found) {
      NS_ASSERTION(found->mStr.Equals(aOperator), "bad tree");
      *aLeftSpace = found->mLeftSpace;
      *aRightSpace = found->mRightSpace;
      *aFlags &= 0xFFFFFFFC; // (= ~0x3) clear the form bits
      *aFlags |= found->mFlags; // just add bits without overwriting
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
nsMathMLOperators::IsMutableOperator(const nsString& aOperator)
{
  NS_ASSERTION(gOperatorTree, "no lookup table, needs addref");
  if (gOperatorTree) {

    gOperatorFound[NS_MATHML_OPERATOR_FORM_INFIX] = nsnull;
    gOperatorFound[NS_MATHML_OPERATOR_FORM_POSTFIX] = nsnull;
    gOperatorFound[NS_MATHML_OPERATOR_FORM_PREFIX] = nsnull;

    OperatorNode node(aOperator, 0);
    OperatorNode* found = (OperatorNode*)gOperatorTree->FindItem(&node);

    // if the operator was found, gOperatorFound contains all the variants 
    // of the operator. check if there is one that meets the criteria

    found = gOperatorFound[NS_MATHML_OPERATOR_FORM_INFIX];
    if (found && 
        (NS_MATHML_OPERATOR_IS_STRETCHY(found->mFlags) || 
         NS_MATHML_OPERATOR_IS_LARGEOP(found->mFlags)))
    {
      return PR_TRUE;
    }

    found = gOperatorFound[NS_MATHML_OPERATOR_FORM_POSTFIX];
    if (found && 
        (NS_MATHML_OPERATOR_IS_STRETCHY(found->mFlags) || 
         NS_MATHML_OPERATOR_IS_LARGEOP(found->mFlags)))
    {
      return PR_TRUE;
    }

    found = gOperatorFound[NS_MATHML_OPERATOR_FORM_PREFIX];
    if (found && 
        (NS_MATHML_OPERATOR_IS_STRETCHY(found->mFlags) || 
         NS_MATHML_OPERATOR_IS_LARGEOP(found->mFlags)))
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


#if 0
// DEBUG
// BEGIN TEST CODE: 
// code used to test the dictionary
//#include <stdio.h>

void DEBUG_PrintString(const nsString aString) 
{
  for (PRInt32 i = 0; i<aString.Length(); i++) {
    PRUnichar ch = aString.CharAt(i);
    if (ch < 0x00FF)
        PRINTF("%c", char(ch));
    else
        PRINTF("[0x%04X]", ch);
  }
}

static const char* kJunkOperator[] = {
  nsnull,
  "",
  ")(",
  "#",
  "<!",
  "<@>"
};

// define an array of all the flags
#define MATHML_OPERATOR(_i,_operator,_flags,_lspace,_rspace) nsOperatorFlags(_flags),
const nsOperatorFlags kMathMLOperatorFlags[] = {
#include "nsMathMLOperatorList.h"
};
#undef MATHML_OPERATOR

int 
TestOperators() {
  AddRefTable();
  	
  int rv = 0;

  // First make sure we can find all of the operators that are supposed to
  // be in the table.
  extern const PRUnichar kMathMLOperator[];

  PRBool found;

  nsOperatorFlags flags, form;
  float lspace, rspace;

  PRINTF("\nChecking the operator dictionary...\n");
    
  nsAutoString aOperator;
  nsStr::Initialize(aOperator, eTwoByte);

  int i = 0;
  const PRUnichar* cp = &kMathMLOperator[0];
  while (i < NS_MATHML_OPERATOR_COUNT) {

    aOperator = *cp++;
    while (*cp) { aOperator.Append(*cp++); }
    cp++; // skip null separator

    form = NS_MATHML_OPERATOR_GET_FORM(kMathMLOperatorFlags[i]);
    found = nsMathMLOperators::LookupOperator(aOperator, form, 
                               &flags, &lspace, &rspace);       

    if (!found) {
        PRINTF("bug: can't find operator="); DEBUG_PrintString(aOperator);
      rv = -1;
    }
    if (flags != kMathMLOperatorFlags[i]) {
        PRINTF("bug: operator="); DEBUG_PrintString(aOperator);
        PRINTF(" .... flags are wrong\n");
      getchar();
      rv = -1;
    }
    i++;

    if (i<10) { 
      DEBUG_PrintString(aOperator);
      PRINTF(" tested. Press return to continue...");
      getchar();
    }

  }

  // Now make sure we don't find some garbage
  for (int j = 0; j < (int) (sizeof(kJunkOperator) / sizeof(const char*)); j++) {
    const char* name = kJunkOperator[j];    
    found = nsMathMLOperators::LookupOperator(nsCAutoString(name), form, 
                               &flags, &lspace, &rspace);
    if (found) {
        PRINTF("bug: found '%s'\n", name ? name : "(null)");
      rv = -1;
    }
  }

  PRINTF("%d operators.",i);
  PRINTF((rv == 0)? " All tests passed!\n" : " ERROR *** Corrupted Dictionary\n");

  ReleaseTable();
  return rv;
}
// END TEST CODE
#endif

