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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsCOMPtr.h"
#include "nsHTMLReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsStyleConsts.h"
#include "nsFrame.h"
#include "nsContainerFrame.h"
#include "nsLayoutAtoms.h"

nsresult
NS_NewHTMLReflowCommand(nsHTMLReflowCommand**           aInstancePtrResult,
                        nsIFrame*                       aTargetFrame,
                        nsReflowType                    aReflowType,
                        nsIFrame*                       aChildFrame,
                        nsIAtom*                        aAttribute)
{
  NS_ASSERTION(aInstancePtrResult,
               "null result passed to NS_NewHTMLReflowCommand");

  *aInstancePtrResult = new nsHTMLReflowCommand(aTargetFrame, aReflowType,
                                                aChildFrame, aAttribute);
  if (!*aInstancePtrResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

#ifdef DEBUG_jesup
PRInt32 gReflows;
PRInt32 gReflowsInUse;
PRInt32 gReflowsInUseMax;
PRInt32 gReflowsZero;
PRInt32 gReflowsAuto;
PRInt32 gReflowsLarger;
PRInt32 gReflowsMaxZero;
PRInt32 gReflowsMaxAuto;
PRInt32 gReflowsMaxLarger;
class mPathStats {
public:
  mPathStats();
  ~mPathStats();
};
mPathStats::mPathStats()
{
}
mPathStats::~mPathStats()
{
  printf("nsHTMLReflowCommand->mPath stats:\n");
  printf("\tNumber created:   %d\n",gReflows);
  printf("\tNumber in-use:    %d\n",gReflowsInUseMax);

  printf("\tNumber size == 0: %d\n",gReflowsZero);
  printf("\tNumber size <= 8: %d\n",gReflowsAuto);
  printf("\tNumber size  > 8: %d\n",gReflowsLarger);
  printf("\tNum max size == 0: %d\n",gReflowsMaxZero);
  printf("\tNum max size <= 8: %d\n",gReflowsMaxAuto);
  printf("\tNum max size  > 8: %d\n",gReflowsMaxLarger);
}

// Just so constructor/destructor's get called
mPathStats gmPathStats;
#endif

// Construct a reflow command given a target frame, reflow command type,
// and optional child frame
nsHTMLReflowCommand::nsHTMLReflowCommand(nsIFrame*    aTargetFrame,
                                         nsReflowType aReflowType,
                                         nsIFrame*    aChildFrame,
                                         nsIAtom*     aAttribute)
  : mType(aReflowType), mTargetFrame(aTargetFrame), mChildFrame(aChildFrame),
    mAttribute(aAttribute),
    mListName(nsnull),
    mFlags(0)
{
  MOZ_COUNT_CTOR(nsHTMLReflowCommand);
  NS_PRECONDITION(mTargetFrame != nsnull, "null target frame");
  if (nsnull!=mAttribute)
    NS_ADDREF(mAttribute);
#ifdef DEBUG_jesup
  gReflows++;
  gReflowsInUse++;
  if (gReflowsInUse > gReflowsInUseMax)
    gReflowsInUseMax = gReflowsInUse;
#endif
}

nsHTMLReflowCommand::~nsHTMLReflowCommand()
{
  MOZ_COUNT_DTOR(nsHTMLReflowCommand);
#ifdef DEBUG_jesup
  if (mPath.GetArraySize() == 0)
    gReflowsMaxZero++;
  else if (mPath.GetArraySize() <= 8)
    gReflowsMaxAuto++;
  else
    gReflowsMaxLarger++;
  gReflowsInUse--;
#endif

  NS_IF_RELEASE(mAttribute);
  NS_IF_RELEASE(mListName);
}

nsresult
nsHTMLReflowCommand::List(FILE* out) const
{
#ifdef DEBUG
  static const char* kReflowCommandType[] = {
    "ContentChanged",
    "StyleChanged",
    "ReflowDirty",
    "UserDefined",
  };

  fprintf(out, "ReflowCommand@%p[%s]:",
          this, kReflowCommandType[mType]);
  if (mTargetFrame) {
    fprintf(out, " target=");
    nsFrame::ListTag(out, mTargetFrame);
  }
  if (mChildFrame) {
    fprintf(out, " child=");
    nsFrame::ListTag(out, mChildFrame);
  }
  if (mAttribute) {
    fprintf(out, " attr=");
    nsAutoString attr;
    mAttribute->ToString(attr);
    fputs(NS_LossyConvertUCS2toASCII(attr).get(), out);
  }
  if (mListName) {
    fprintf(out, " list=");
    nsAutoString attr;
    mListName->ToString(attr);
    fputs(NS_LossyConvertUCS2toASCII(attr).get(), out);
  }
  fprintf(out, "\n");

  // Show the path, but without using mPath which is in an undefined
  // state at this point.
  if (mTargetFrame) {
    PRBool didOne = PR_FALSE;
    for (nsIFrame* f = mTargetFrame; f; f = f->GetParent()) {
      if (f != mTargetFrame) {
        fprintf(out, " ");
        nsFrame::ListTag(out, f);
        didOne = PR_TRUE;
      }
    }
    if (didOne) {
      fprintf(out, "\n");
    }
  }
#endif
  return NS_OK;
}
