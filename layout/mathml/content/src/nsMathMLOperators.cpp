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

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"

#include "nsIComponentManager.h"
#include "nsIPersistentProperties2.h"
#include "nsNetUtil.h"
#include "nsIURI.h"

#include "nsMathMLOperators.h"

// operator dictionary entry
struct OperatorData {
  OperatorData(void)
    : mFlags(0),
      mLeftSpace(0.0f),
      mRightSpace(0.0f)
  {
  }

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
static OperatorData*   gOperatorFound[4];

static PRInt32         gTableRefCount = 0;
static PRInt32         gOperatorCount = 0;
static OperatorData*   gOperatorArray = nsnull;
static nsHashtable*    gOperatorTable = nsnull;
static nsVoidArray*    gStretchyOperatorArray = nsnull;
static nsStringArray*  gInvariantChar = nsnull;
static PRBool          gInitialized   = PR_FALSE;

static const PRUnichar kNullCh  = PRUnichar('\0');
static const PRUnichar kDashCh  = PRUnichar('#');
static const PRUnichar kEqualCh = PRUnichar('=');
static const PRUnichar kColonCh = PRUnichar(':');

static const char* const kMathVariant_name[] = {
  "normal",
  "bold",
  "italic",
  "bold-italic",
  "sans-serif",
  "bold-sans-serif",
  "sans-serif-italic",
  "sans-serif-bold-italic",
  "monospace",
  "script",
  "bold-script",
  "fraktur",
  "bold-fraktur",
  "double-struck"
};

void
SetProperty(OperatorData* aOperatorData,
            nsString      aName,
            nsString      aValue)
{
  if (!aName.Length() || !aValue.Length())
    return;

  // XXX These ones are not kept in the dictionary
  // Support for these requires nsString member variables 
  // maxsize (default: infinity)
  // minsize (default: 1)

  if (aValue.EqualsWithConversion("true")) {
    // see if we should enable flags with default value=false
    if (aName.EqualsWithConversion("fence"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_FENCE;
    else if (aName.EqualsWithConversion("accent"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_ACCENT;
    else if (aName.EqualsWithConversion("largeop"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_LARGEOP;
    else if (aName.EqualsWithConversion("separator"))
      aOperatorData->mFlags |=  NS_MATHML_OPERATOR_SEPARATOR;
    else if (aName.EqualsWithConversion("movablelimits"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_MOVABLELIMITS;
  }
  else if (aValue.EqualsWithConversion("false")) {
    // see if we should disable flags with default value=true
    if (aName.EqualsWithConversion("symmetric"))
      aOperatorData->mFlags &= ~NS_MATHML_OPERATOR_SYMMETRIC;
  }
  else if (aName.EqualsWithConversion("stretchy") &&
          (1 == aOperatorData->mStr.Length())) {
    if (aValue.EqualsWithConversion("vertical"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_STRETCHY_VERT;
    else if (aValue.EqualsWithConversion("horizontal"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_STRETCHY_HORIZ;
    else return; // invalid value
    if (kNotFound == nsMathMLOperators::FindStretchyOperator(aOperatorData->mStr[0])) {
      gStretchyOperatorArray->AppendElement(aOperatorData);
    }
  }
  else {
    PRInt32 i = 0;
    float space = 0.0f;
    PRBool isLeftSpace;
    if (aName.EqualsWithConversion("lspace"))
      isLeftSpace = PR_TRUE;
    else if (aName.EqualsWithConversion("rspace"))
      isLeftSpace = PR_FALSE;
    else return;  // input is not applicable

    // See if it is a numeric value (unit is assumed to be 'em')
    if (nsCRT::IsAsciiDigit(aValue[0])) {
      PRInt32 error = 0;
      space = aValue.ToFloat(&error);
      if (error) return;
    }
    // See if it is one of the 'namedspace' (ranging 1/18em...7/18em)
    else if (aValue.EqualsWithConversion("veryverythinmathspace"))  i = 1;
    else if (aValue.EqualsWithConversion("verythinmathspace"))      i = 2;
    else if (aValue.EqualsWithConversion("thinmathspace"))          i = 3;
    else if (aValue.EqualsWithConversion("mediummathspace"))        i = 4;
    else if (aValue.EqualsWithConversion("thickmathspace"))         i = 5;
    else if (aValue.EqualsWithConversion("verythickmathspace"))     i = 6;
    else if (aValue.EqualsWithConversion("veryverythickmathspace")) i = 7;

    if (0 != i) // it was a namedspace value
      space = float(i)/float(18);

    if (isLeftSpace)
      aOperatorData->mLeftSpace = space;
    else
      aOperatorData->mRightSpace = space;
  }
}

PRBool
SetOperator(OperatorData*   aOperatorData,
            nsOperatorFlags aForm,
            nsString        aOperator,
            nsString        aAttributes)

{
  // aOperator is in the expanded format \uNNNN\uNNNN ...
  // First compress these Unicode points to the internal nsString format
  PRInt32 i = 0;
  nsAutoString name, value;
  PRInt32 len = aOperator.Length();
  PRUnichar c = aOperator[i++];
  PRUint32 state  = 0;
  PRUnichar uchar = 0;
  while (i <= len) {
    if (0 == state) {
      if (c != '\\')
        return PR_FALSE;
      if (i < len)
        c = aOperator[i];
      i++;
      if (('u' != c) && ('U' != c))
        return PR_FALSE;
      if (i < len)
        c = aOperator[i];
      i++;
      state++;
    }
    else {
      if (('0' <= c) && (c <= '9'))
         uchar = (uchar << 4) | (c - '0');
      else if (('a' <= c) && (c <= 'f'))
         uchar = (uchar << 4) | (c - 'a' + 0x0a);
      else if (('A' <= c) && (c <= 'F'))
         uchar = (uchar << 4) | (c - 'A' + 0x0a);
      else return PR_FALSE;
      if (i < len)
        c = aOperator[i];
      i++;
      state++;
      if (5 == state) {
        value.Append(uchar);
        uchar = 0;
        state = 0;
      }
    }
  }
  if (0 != state) return PR_FALSE;

  // Quick return when the caller doesn't care about the attributes and just wants
  // to know if this is a valid operator (this is the case at the first pass of the
  // parsing of the dictionary in InitOperators())
  if (!aForm) return PR_TRUE;

  // Add operator to hash table (symmetric="true" by default for all operators)
  aOperatorData->mFlags |= aForm | NS_MATHML_OPERATOR_SYMMETRIC;
  aOperatorData->mStr.Assign(value);
  value.AppendInt(aForm, 10);
  nsStringKey key(value);
  gOperatorTable->Put(&key, aOperatorData);

#ifdef NS_DEBUG
  NS_LossyConvertUCS2toASCII str(aAttributes);
#endif
  // Loop over the space-delimited list of attributes to get the name:value pairs
  aAttributes.Append(kNullCh);  // put an extra null at the end
  PRUnichar* start = (PRUnichar*)(const PRUnichar*)aAttributes.get();
  PRUnichar* end   = start;
  while ((kNullCh != *start) && (kDashCh != *start)) {
    name.SetLength(0);
    value.SetLength(0);
    // skip leading space, the dash amounts to the end of the line
    while ((kNullCh!=*start) && (kDashCh!=*start) && nsCRT::IsAsciiSpace(*start)) {
      ++start;
    }
    end = start;
    // look for ':' or '='
    while ((kNullCh!=*end) && (kDashCh!=*end) && (kColonCh!=*end) && (kEqualCh!=*end)) {
      ++end;
    }
    if ((kColonCh!=*end) && (kEqualCh!=*end)) {
#ifdef NS_DEBUG
      printf("Bad MathML operator: %s\n", str.get());
#endif
      return PR_TRUE;
    }
    *end = kNullCh; // end segment here
    // this segment is the name
    if (start < end) {
      name.Assign(start);
    }
    start = ++end;
    // look for space or end of line
    while ((kNullCh!=*end) && (kDashCh!=*start) && !nsCRT::IsAsciiSpace(*end)) {
      ++end;
    }
    *end = kNullCh; // end segment here
    // this segment is the value
    if (start < end) {
      value.Assign(start);
    }
    SetProperty(aOperatorData, name, value);
    start = ++end;
  }
  return PR_TRUE;
}

nsresult
InitOperators(void)
{
  // Load the property file containing the Operator Dictionary
  nsresult rv;
  nsAutoString uriStr;
  nsCOMPtr<nsIURI> uri;
  uriStr.AssignWithConversion("resource:/res/fonts/mathfont.properties");
  rv = NS_NewURI(getter_AddRefs(uri), uriStr);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIInputStream> in;
  rv = NS_OpenURI(getter_AddRefs(in), uri);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIPersistentProperties> mathfontProp;
  rv = nsComponentManager::
       CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID, nsnull,
                      NS_GET_IID(nsIPersistentProperties),
                      getter_AddRefs(mathfontProp));
  if NS_FAILED(rv) return rv;
  rv = mathfontProp->Load(in);
  if NS_FAILED(rv) return rv;

  // Get the list of invariant chars
  for (PRInt32 i = 0; i < nsMathMLOperators::eMATHVARIANT_COUNT; ++i) {
    nsAutoString key, value;
    key.Assign(NS_LITERAL_STRING("mathvariant."));
    key.AppendWithConversion(kMathVariant_name[i]);
    mathfontProp->GetStringProperty(key, value);
    gInvariantChar->AppendString(value); // i.e., gInvariantChar[i] holds this list
  }

  // Parse the Operator Dictionary in two passes.
  // The first pass is to count the number of operators; the second pass is to
  // allocate the necessary space for them and to add them in the hash table.
  for (PRInt32 pass = 1; pass <= 2; pass++) {
    OperatorData dummyData;
    OperatorData* operatorData = &dummyData;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    if (NS_SUCCEEDED(mathfontProp->SimpleEnumerateProperties(getter_AddRefs(iterator)))) {
      PRBool more;
      PRInt32 index = 0;
      nsAutoString name, attributes;
      while ((NS_SUCCEEDED(iterator->HasMoreElements(&more))) && more) {
        nsCOMPtr<nsIPropertyElement> element;
        if (NS_SUCCEEDED(iterator->GetNext(getter_AddRefs(element)))) {
          nsXPIDLString xkey, xvalue;
          if (NS_SUCCEEDED(element->GetKey(getter_Copies(xkey))) &&
              NS_SUCCEEDED(element->GetValue(getter_Copies(xvalue)))) {
            name.Assign(xkey);
            attributes.Assign(xvalue);
            // expected key: operator.\uNNNN.{infix,postfix,prefix}
            if ((21 <= name.Length()) && (0 == name.Find("operator.\\u"))) {
              name.Cut(0, 9); // 9 is the length of "operator.";
              PRInt32 len = name.Length();
              nsOperatorFlags form = 0;
              if (kNotFound != name.RFind(".infix")) {
                form = NS_MATHML_OPERATOR_FORM_INFIX;
                len -= 6;  // 6 is the length of ".infix";
              }
              else if (kNotFound != name.RFind(".postfix")) {
                form = NS_MATHML_OPERATOR_FORM_POSTFIX;
                len -= 8; // 8 is the length of ".postfix";
              }
              else if (kNotFound != name.RFind(".prefix")) {
                form = NS_MATHML_OPERATOR_FORM_PREFIX;
                len -= 7; // 7 is the length of ".prefix";
              }
              else continue; // input is not applicable
              name.SetLength(len);
              if (2 == pass) { // allocate space and start the storage
                if (!gOperatorArray) {
                  if (0 == gOperatorCount) return NS_ERROR_UNEXPECTED;
                  gOperatorArray = new OperatorData[gOperatorCount];
                  if (!gOperatorArray) return NS_ERROR_OUT_OF_MEMORY;
                }
                operatorData = &gOperatorArray[index];
              }
              else {
                form = 0; // to quickly return from SetOperator() at pass 1
              }
              // See if the operator should be retained
              if (SetOperator(operatorData, form, name, attributes)) {
                index++;
                if (1 == pass) gOperatorCount = index;
              }
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
InitGlobals()
{
  gInitialized = PR_TRUE;
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  gInvariantChar = new nsStringArray();
  gStretchyOperatorArray = new nsVoidArray();
  if (gInvariantChar && gStretchyOperatorArray) {
    gOperatorTable = new nsHashtable();
    if (gOperatorTable) {
      rv = InitOperators();
    }
  }
  if (NS_FAILED(rv)) {
    if (gInvariantChar) {
      delete gInvariantChar;
      gInvariantChar = nsnull;
    }
    if (gOperatorArray) {
      delete[] gOperatorArray;
      gOperatorArray = nsnull;
    }
    if (gStretchyOperatorArray) {
      delete gStretchyOperatorArray;
      gStretchyOperatorArray = nsnull;
    }
    if (gOperatorTable) {
      delete gOperatorTable;
      gOperatorTable = nsnull;
    }
  }
  return rv;
}

void
nsMathMLOperators::AddRefTable(void)
{
  gTableRefCount++;
}

void
nsMathMLOperators::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    if (gOperatorArray) {
      delete[] gOperatorArray;
      gOperatorArray = nsnull;
    }
    if (gStretchyOperatorArray) {
      delete gStretchyOperatorArray;
      gStretchyOperatorArray = nsnull;
    }
    if (gOperatorTable) {
      delete gOperatorTable;
      gOperatorTable = nsnull;
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
  if (!gInitialized) {
    InitGlobals();
  }
  if (gOperatorTable) {
    NS_ASSERTION(aFlags && aLeftSpace && aRightSpace, "bad usage");
    NS_ASSERTION(aForm>=0 && aForm<4, "*** invalid call ***");

    OperatorData* found;
    PRInt32 form = NS_MATHML_OPERATOR_GET_FORM(aForm);
    gOperatorFound[NS_MATHML_OPERATOR_FORM_INFIX] = nsnull;
    gOperatorFound[NS_MATHML_OPERATOR_FORM_POSTFIX] = nsnull;
    gOperatorFound[NS_MATHML_OPERATOR_FORM_PREFIX] = nsnull;

    nsAutoString key(aOperator);
    key.AppendInt(form, 10);
    nsStringKey hashkey(key);
    gOperatorFound[form] = found = (OperatorData*)gOperatorTable->Get(&hashkey);

    // If not found, check if the operator exists perhaps in a different form,
    // in the order of preference: infix, postfix, prefix
    if (!found) {
      if (form != NS_MATHML_OPERATOR_FORM_INFIX) {
        form = NS_MATHML_OPERATOR_FORM_INFIX;
        key.Assign(aOperator);
        key.AppendInt(form, 10);
        nsStringKey hashkey(key);
        gOperatorFound[form] = found = (OperatorData*)gOperatorTable->Get(&hashkey);
      }
      if (!found) {
        if (form != NS_MATHML_OPERATOR_FORM_POSTFIX) {
          form = NS_MATHML_OPERATOR_FORM_POSTFIX;
          key.Assign(aOperator);
          key.AppendInt(form, 10);
          nsStringKey hashkey(key);
          gOperatorFound[form] = found = (OperatorData*)gOperatorTable->Get(&hashkey);
        }
        if (!found) {
          if (form != NS_MATHML_OPERATOR_FORM_PREFIX) {
            form = NS_MATHML_OPERATOR_FORM_PREFIX;
            key.Assign(aOperator);
            key.AppendInt(form, 10);
            nsStringKey hashkey(key);
            gOperatorFound[form] = found = (OperatorData*)gOperatorTable->Get(&hashkey);
          }
        }
      }
    }
    if (found) {
      NS_ASSERTION(found->mStr.Equals(aOperator), "bad setup");
      *aLeftSpace = found->mLeftSpace;
      *aRightSpace = found->mRightSpace;
      *aFlags &= ~NS_MATHML_OPERATOR_FORM; // clear the form bits
      *aFlags |= found->mFlags; // just add bits without overwriting
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
nsMathMLOperators::IsMutableOperator(const nsString& aOperator)
{
  if (!gInitialized) {
    InitGlobals();
  }
  if (gOperatorTable) {
    // Lookup with form=0 will put all the variants in gOperatorFound[]
    float dummy;
    nsOperatorFlags flags = 0;
    LookupOperator(aOperator, 0, &flags, &dummy, &dummy);
    // if the operator was found, gOperatorFound contains all the variants
    // of the operator. check if there is one that meets the criteria
    OperatorData* found;
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

PRInt32
nsMathMLOperators::CountStretchyOperator()
{
  if (!gInitialized) {
    InitGlobals();
  }
  return (gStretchyOperatorArray) ? gStretchyOperatorArray->Count() : 0;
}

PRInt32
nsMathMLOperators::FindStretchyOperator(PRUnichar aOperator)
{
  if (!gInitialized) {
    InitGlobals();
  }
  if (gStretchyOperatorArray) {
    for (PRInt32 k = 0; k < gStretchyOperatorArray->Count(); k++) {
      OperatorData* data = (OperatorData*)gStretchyOperatorArray->ElementAt(k);
      if (data && (aOperator == data->mStr[0])) {
        return k;
      }
    }
  }
  return kNotFound;
}

nsStretchDirection
nsMathMLOperators::GetStretchyDirectionAt(PRInt32 aIndex)
{
  NS_ASSERTION(gStretchyOperatorArray, "invalid call");
  if (gStretchyOperatorArray) {
    NS_ASSERTION(aIndex < gStretchyOperatorArray->Count(), "invalid call");
    OperatorData* data = (OperatorData*)gStretchyOperatorArray->ElementAt(aIndex);
    if (data) {
      if (NS_MATHML_OPERATOR_IS_STRETCHY_VERT(data->mFlags))
        return NS_STRETCH_DIRECTION_VERTICAL;
      else if (NS_MATHML_OPERATOR_IS_STRETCHY_HORIZ(data->mFlags))
        return NS_STRETCH_DIRECTION_HORIZONTAL;
      NS_ASSERTION(PR_FALSE, "*** bad setup ***");
    }
  }
  return NS_STRETCH_DIRECTION_UNSUPPORTED;
}

void
nsMathMLOperators::DisableStretchyOperatorAt(PRInt32 aIndex)
{
  NS_ASSERTION(gStretchyOperatorArray, "invalid call");
  if (gStretchyOperatorArray) {
    NS_ASSERTION(aIndex < gStretchyOperatorArray->Count(), "invalid call");
    gStretchyOperatorArray->ReplaceElementAt(nsnull, aIndex);
  }
}

PRBool
nsMathMLOperators::LookupInvariantChar(PRUnichar     aChar,
                                       eMATHVARIANT* aType)
{
  if (!gInitialized) {
    InitGlobals();
  }
  if (aType) *aType = eMATHVARIANT_NONE;
  if (gInvariantChar) {
    for (PRInt32 i = 0; i < gInvariantChar->Count(); ++i) {
      nsAutoString list;
      gInvariantChar->StringAt(i, list);
      if (kNotFound != list.FindChar(aChar)) {
         if (aType) *aType = eMATHVARIANT(i);
         return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}
