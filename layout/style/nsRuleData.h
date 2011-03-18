/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

/*
 * temporary (expanded) representation of property-value pairs used to
 * hold data from matched rules during style data computation.
 */

#ifndef nsRuleData_h_
#define nsRuleData_h_

#include "nsCSSProps.h"
#include "nsCSSStruct.h"
#include "nsStyleStructFwd.h"

class nsPresContext;
class nsStyleContext;
struct nsRuleData;

typedef void (*nsPostResolveFunc)(void* aStyleStruct, nsRuleData* aData);

struct nsRuleData
{
  PRUint32 mSIDs;
  PRPackedBool mCanStoreInRuleTree;
  PRPackedBool mIsImportantRule;
  PRUint8 mLevel; // an nsStyleSet::sheetType
  nsPresContext* mPresContext;
  nsStyleContext* mStyleContext;
  nsPostResolveFunc mPostResolveCallback;

  // Should always be stack-allocated! We don't own these structures!
  nsRuleDataFont* mFontData;
  nsRuleDataDisplay* mDisplayData;
  nsRuleDataMargin* mMarginData;
  nsRuleDataList* mListData;
  nsRuleDataPosition* mPositionData;
  nsRuleDataTable* mTableData;
  nsRuleDataColor* mColorData;
  nsRuleDataContent* mContentData;
  nsRuleDataText* mTextData;
  nsRuleDataUserInterface* mUserInterfaceData;
  nsRuleDataXUL* mXULData;
  nsRuleDataSVG* mSVGData;
  nsRuleDataColumn* mColumnData;

  nsRuleData(PRUint32 aSIDs,
             nsPresContext* aContext,
             nsStyleContext* aStyleContext)
    : mSIDs(aSIDs),
      mCanStoreInRuleTree(PR_TRUE),
      mPresContext(aContext),
      mStyleContext(aStyleContext),
      mPostResolveCallback(nsnull),
      mFontData(nsnull),
      mDisplayData(nsnull),
      mMarginData(nsnull),
      mListData(nsnull),
      mPositionData(nsnull),
      mTableData(nsnull),
      mColorData(nsnull),
      mContentData(nsnull),
      mTextData(nsnull),
      mUserInterfaceData(nsnull),
      mXULData(nsnull),
      mSVGData(nsnull),
      mColumnData(nsnull)
  {}
  ~nsRuleData() {}

  /**
   * Return a pointer to the value object within |this| corresponding
   * to property |aProperty|.
   *
   * This function must only be called if the given property is in
   * mSIDs.
   */
  nsCSSValue* ValueFor(nsCSSProperty aProperty);

  /**
   * Getters like ValueFor(aProperty), but for each property by name
   * (ValueForBackgroundColor, etc.), and more efficient than ValueFor.
   * These use the names used for the property on DOM interfaces (the
   * 'method' field in nsCSSPropList.h).
   *
   * Like ValueFor(), the caller must check that the property is within
   * mSIDs.
   */
  #define CSS_PROP_INCLUDE_NOT_CSS
  #define CSS_PROP_DOMPROP_PREFIXED(prop_) prop_
  #define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_,        \
                   parsevariant_, kwtable_, stylestruct_, stylestructoffset_,\
                   animtype_)                                                \
    nsCSSValue* ValueFor##method_() {                                        \
      NS_ABORT_IF_FALSE(mSIDs & NS_STYLE_INHERIT_BIT(stylestruct_),          \
                        "Calling nsRuleData::ValueFor" #method_ " without "  \
                        "NS_STYLE_INHERIT_BIT(" #stylestruct_ " in mSIDs."); \
      nsRuleData##datastruct_ *cssstruct = m##datastruct_##Data;             \
      NS_ABORT_IF_FALSE(cssstruct, "nsRuleNode::Get" #stylestruct_ "Data "   \
                                   "set up nsRuleData incorrectly");         \
      return &cssstruct->member_;                                            \
    }                                                                        \
    const nsCSSValue* ValueFor##method_() const {                            \
      NS_ABORT_IF_FALSE(mSIDs & NS_STYLE_INHERIT_BIT(stylestruct_),          \
                        "Calling nsRuleData::ValueFor" #method_ " without "  \
                        "NS_STYLE_INHERIT_BIT(" #stylestruct_ " in mSIDs."); \
      const nsRuleData##datastruct_ *cssstruct = m##datastruct_##Data;       \
      NS_ABORT_IF_FALSE(cssstruct, "nsRuleNode::Get" #stylestruct_ "Data "   \
                                   "set up nsRuleData incorrectly");         \
      return &cssstruct->member_;                                            \
    }
  #define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, datastruct_,     \
                               member_, parsevariant_, kwtable_)             \
    /* empty; backend-only structs are not in nsRuleData  */
  #include "nsCSSPropList.h"
  #undef CSS_PROP_INCLUDE_NOT_CSS
  #undef CSS_PROP
  #undef CSS_PROP_DOMPROP_PREFIXED
  #undef CSS_PROP_BACKENDONLY

};

#endif
