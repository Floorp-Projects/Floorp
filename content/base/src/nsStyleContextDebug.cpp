/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// IMPORTANT:
//   This is not a stand-alone source file.  It is included in nsStyleContext.cpp
//

//=========================================================================================================

#ifdef XP_MAC
#include <Events.h>
#include <Timer.h>
static PRBool MacKeyDown(unsigned char theKey)
{
	KeyMap map;
	GetKeys(map);
	return ((*((unsigned char *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}
#endif


static PRBool IsTimeToDumpDebugData()
{
  PRBool timeToDump = PR_FALSE;
#ifdef XP_MAC
	static unsigned long lastTicks = 0;
  if (MacKeyDown(0x3b)) { // control key
    if ((unsigned long)(::TickCount() - lastTicks) > 60) {
	    lastTicks = ::TickCount();
	    timeToDump = PR_TRUE;
	  }
	}
#endif
  return timeToDump;
}

#ifdef XP_MAC
#pragma mark -
#endif
//=========================================================================================================

#ifdef LOG_STYLE_STRUCTS

static void LogStyleStructs(nsStyleContextData* aStyleContextData)
{
#define max_structs eStyleStruct_Max

  static unsigned long totalCount = 0;
  static unsigned long defaultStruct[max_structs];
  static unsigned long setFromParent[max_structs];
  static unsigned long gotMutable[max_structs];
  static unsigned long gotMutableAndDefault[max_structs];

	static PRBool resetCounters = PR_TRUE;

  if (IsTimeToDumpDebugData()) {
    resetCounters = PR_TRUE;
    printf("\n\n\n");
    printf("-------------------------------------------------------------------------------------------------------------\n");
    printf("Count of nsStyleContextData: %ld\n", totalCount);
    printf("-------------------------------------------------------------------------------------------------------------\n");
	  printf("                                unchanged   unchanged%c   set-from-parent%c   size-of-struct  potential-gain-Kb gotMutable/total gotMutableAndDefault/default\n", '%', '%');
    unsigned long totalFootprint = 0;
    unsigned long totalPotentialGain = 0;
    for (short i = 0; i < max_structs; i ++) {
      short index = i+1;
      short sizeOfStruct = 0;
      unsigned long footprint = 0;
      unsigned long potentialGain = 0;
      switch (index) {
		    case eStyleStruct_Font:      			printf("eStyleStruct_Font           "); sizeOfStruct = sizeof(StyleFontImpl);  break;
		    case eStyleStruct_Color:      		printf("eStyleStruct_Color          "); sizeOfStruct = sizeof(StyleColorImpl);  break;
		    case eStyleStruct_List:      			printf("eStyleStruct_List           "); sizeOfStruct = sizeof(StyleListImpl);  break;
		    case eStyleStruct_Position:      	printf("eStyleStruct_Position       "); sizeOfStruct = sizeof(StylePositionImpl);  break;
		    case eStyleStruct_Text:      			printf("eStyleStruct_Text           "); sizeOfStruct = sizeof(StyleTextImpl);  break;
		    case eStyleStruct_Display:      	printf("eStyleStruct_Display        "); sizeOfStruct = sizeof(StyleDisplayImpl);  break;
		    case eStyleStruct_Table:      		printf("eStyleStruct_Table          "); sizeOfStruct = sizeof(StyleTableImpl);  break;
		    case eStyleStruct_Content:      	printf("eStyleStruct_Content        "); sizeOfStruct = sizeof(StyleContentImpl);  break;
		    case eStyleStruct_UserInterface:  printf("eStyleStruct_UserInterface  "); sizeOfStruct = sizeof(StyleUserInterfaceImpl);  break;
		    case eStyleStruct_Print: 					printf("eStyleStruct_Print          "); sizeOfStruct = sizeof(StylePrintImpl);  break;
		    case eStyleStruct_Margin: 				printf("eStyleStruct_Margin         "); sizeOfStruct = sizeof(StyleMarginImpl);  break;
		    case eStyleStruct_Padding: 				printf("eStyleStruct_Padding        "); sizeOfStruct = sizeof(StylePaddingImpl);  break;
		    case eStyleStruct_Border: 				printf("eStyleStruct_Border         "); sizeOfStruct = sizeof(StyleBorderImpl);  break;
		    case eStyleStruct_Outline: 				printf("eStyleStruct_Outline        "); sizeOfStruct = sizeof(StyleOutlineImpl);  break;
#ifdef INCLUDE_XUL
		    case eStyleStruct_XUL: 				    printf("eStyleStruct_Outline        "); sizeOfStruct = sizeof(StyleXULImpl);  break;
#endif
        //#insert new style structs here#
      }
		  short percentDefault = (totalCount == 0 ? 0 : ((100 * defaultStruct[i]) / totalCount));
		  short percentGotMutable = (totalCount == 0 ? 0 : ((100 * gotMutable[i]) / totalCount));
		  short percentGotMutableAndDefault = (defaultStruct[i] == 0 ? 0 : ((100 * gotMutableAndDefault[i]) / defaultStruct[i]));
	    short percentFromParent = (defaultStruct[i] == 0 ? 0 : ((100 * setFromParent[i]) / defaultStruct[i]));

			footprint = totalCount * sizeOfStruct;
			totalFootprint += footprint;

	    potentialGain = defaultStruct[i] * sizeOfStruct;
	    totalPotentialGain += potentialGain;

	    printf("   %7ld        %3d             %3d             %5d             %5d            %5d                %5d\n", 
	           defaultStruct[i], percentDefault, percentFromParent, sizeOfStruct, potentialGain / 1024, percentGotMutable, percentGotMutableAndDefault);
    }
    printf("-------------------------------------------------------------------------------------------------------------\n");
    printf("Current footprint: %4ld Kb\n", totalFootprint / 1024);
    printf("Potential gain:    %4ld Kb (or %d%c)\n", totalPotentialGain / 1024, totalPotentialGain*100/totalFootprint, '%');
    printf("Would remain:      %4ld Kb\n", (totalFootprint - totalPotentialGain) / 1024);
    printf("-------------------------------------------------------------------------------------------------------------\n");
    printf("These stats come from the nsStyleContextData structures that have been deleted since the last output.\n");
    printf("To get the stats for a particular page: load page, dump stats, load 'about:blank', dump stats again.\n");
    printf("-------------------------------------------------------------------------------------------------------------\n");
    printf("\n\n\n");
  }

	if (resetCounters) {
		resetCounters = PR_FALSE;
		totalCount = 0;
		for (short i = 0; i < max_structs; i ++) {
		  defaultStruct[i] = 0L;
		  gotMutable[i] = 0L;
		  gotMutableAndDefault[i] = 0L;
		  setFromParent[i] = 0L;
		}
	}

  if (!aStyleContextData) {
    printf ("*** aStyleContextData is nil\n");
    return;
  }
 
	totalCount++;
  for (short i = 0; i < max_structs; i ++) {
    short index = i+1;
    switch (index) {
		  case eStyleStruct_Font:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mFont->CalcDifference(aStyleContextData->mFont->mInternalFont) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mFont->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Color:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mColor->CalcDifference(aStyleContextData->mColor->mInternalColor) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mColor->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_List:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mList->CalcDifference(aStyleContextData->mList->mInternalList) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mList->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Position:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mPosition->CalcDifference(aStyleContextData->mPosition->mInternalPosition) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mPosition->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Text:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mText->CalcDifference(aStyleContextData->mText->mInternalText) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mText->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Display:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mDisplay->CalcDifference(aStyleContextData->mDisplay->mInternalDisplay) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mDisplay->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Table:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mTable->CalcDifference(aStyleContextData->mTable->mInternalTable) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mTable->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Content:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mContent->CalcDifference(aStyleContextData->mContent->mInternalContent) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mContent->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_UserInterface:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mUserInterface->CalcDifference(aStyleContextData->mUserInterface->mInternalUserInterface) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mUserInterface->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Print:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mPrint->CalcDifference(aStyleContextData->mPrint->mInternalPrint) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mPrint->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Margin:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mMargin->CalcDifference(aStyleContextData->mMargin->mInternalMargin) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mMargin->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Padding:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mPadding->CalcDifference(aStyleContextData->mPadding->mInternalPadding) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mPadding->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Border:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mBorder->CalcDifference(aStyleContextData->mBorder->mInternalBorder) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mBorder->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
		  case eStyleStruct_Outline:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mOutline->CalcDifference(aStyleContextData->mOutline->mInternalOutline) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mOutline->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
#ifdef INCLUDE_XUL
		  case eStyleStruct_XUL:
	    	if (aStyleContextData->mGotMutable[i])
	    	  gotMutable[i]++;
		    if (aStyleContextData->mXUL->CalcDifference(aStyleContextData->mXUL->mInternalXUL) == NS_STYLE_HINT_NONE) {
		    	defaultStruct[i]++;
	    	  if (aStyleContextData->mGotMutable[i])
	    	    gotMutableAndDefault[i]++;
			    if (aStyleContextData->mXUL->mSetFromParent)
			    	setFromParent[i]++;
		    }
		    break;
#endif
      //#insert new style structs here#
    }
  }

  static short inCount = 0;
  static short outCount = 0;
  if (inCount++ % 1000 == 0) {
    switch (outCount++) {
      case 0:  printf("still logging"); 		 break;
      case 20: printf("\n"); outCount = 0;	 break;
      default: printf(".");  fflush(stdout); break;
    }
  }
}
#endif // LOG_STYLE_STRUCTS


#ifdef XP_MAC
#pragma mark -
#endif
//=========================================================================================================

#ifdef LOG_GET_STYLE_DATA_CALLS

static void LogGetStyleDataCall(nsStyleStructID aSID, LogCallType aLogCallType, nsIStyleContext* aStyleContext, PRBool aEnteringFunction)
{
#define max_structs (eStyleStruct_Max + 1)
#define small_depth_threshold	8

  static unsigned long calls[max_structs*logCallType_Max];
  static unsigned long callspercent[max_structs*logCallType_Max];
  static unsigned long depth[max_structs*logCallType_Max];
  static unsigned long maxdepth[max_structs*logCallType_Max];
  static unsigned long smalldepth[max_structs*logCallType_Max];
  static unsigned long microsecs[logCallType_Max];
  static unsigned long totalMicrosecs;
  static UnsignedWide  startMicrosecs;
  static UnsignedWide  endMicrosecs;

  if (!aEnteringFunction) {
    ::Microseconds(&endMicrosecs);
    totalMicrosecs += endMicrosecs.lo - startMicrosecs.lo;
    microsecs[aLogCallType] += endMicrosecs.lo - startMicrosecs.lo;
    return;
  }


	static PRBool resetCounters = PR_TRUE;

  if (IsTimeToDumpDebugData()) {
    resetCounters = PR_TRUE;
    
    unsigned long totalCalls;
    unsigned long totalMaxdepth;
    unsigned long totalDepth;
    for (short i = 0; i < (max_structs*logCallType_Max); i ++) {

			if (i%max_structs == 0) {
	    	switch (i/max_structs) {
	    	  case 0:
	    	    printf("\n\n\n");
	    	    printf("----GetStyleData--------------------------------------------------------------------------\n");
	    	    printf("                                calls   calls%c        max depth     avg depth   (depth<%d)%\n", '%', small_depth_threshold);
	    	    break;
	    	  case 1:
	    	    printf("----ReadMutableStyleData-------------------------------------------------------------------\n");
	    	    printf("                                calls   calls%c        max depth     avg depth   (depth<%d)%\n", '%', small_depth_threshold);
	    	    break;
	    	  case 2:
	    	    printf("----WriteMutableStyleData------------------------------------------------------------------\n");
	    	    printf("                                calls   calls%c        max depth     avg depth   (depth<%d)%\n", '%', small_depth_threshold);
	    	    break;
	    	  case 3:
	    	    printf("----GetStyle------------------------------------------------------------------------------\n");
	    	    printf("                                calls   calls%c        max depth     avg depth   (depth<%d)%\n", '%', small_depth_threshold);
	    	    break;
	    	}

				totalCalls = totalMaxdepth = totalDepth = 0;
	      for (short j = i; j < i + max_structs; j++) {
					totalCalls += calls[j];
					totalDepth += depth[j];
					if (totalMaxdepth < maxdepth[j]) {
					  totalMaxdepth = maxdepth[j];
					}
	      }
			}

      switch (i%max_structs + 1) {
		    case eStyleStruct_Font:      			printf("eStyleStruct_Font           "); break;
		    case eStyleStruct_Color:      		printf("eStyleStruct_Color          "); break;
		    case eStyleStruct_List:      			printf("eStyleStruct_List           "); break;
		    case eStyleStruct_Position:      	printf("eStyleStruct_Position       "); break;
		    case eStyleStruct_Text:      			printf("eStyleStruct_Text           "); break;
		    case eStyleStruct_Display:      	printf("eStyleStruct_Display        "); break;
		    case eStyleStruct_Table:      		printf("eStyleStruct_Table          "); break;
		    case eStyleStruct_Content:      	printf("eStyleStruct_Content        "); break;
		    case eStyleStruct_UserInterface:  printf("eStyleStruct_UserInterface  "); break;
		    case eStyleStruct_Print: 					printf("eStyleStruct_Print          "); break;
		    case eStyleStruct_Margin: 				printf("eStyleStruct_Margin         "); break;
		    case eStyleStruct_Padding: 				printf("eStyleStruct_Padding        "); break;
		    case eStyleStruct_Border: 				printf("eStyleStruct_Border         "); break;
		    case eStyleStruct_Outline: 				printf("eStyleStruct_Outline        "); break;
#ifdef INCLUDE_XUL
		    case eStyleStruct_XUL: 		    		printf("eStyleStruct_XUL            "); break;
#endif
        //#insert new style structs here#
		    case eStyleStruct_BorderPaddingShortcut: 				printf("BorderPaddingShortcut       "); break;
      }
			short percent = 100*calls[i]/totalCalls;
			short avdepth = calls[i] == 0 ? 0 : round(float(depth[i])/float(calls[i]));
			short smdepth = 100*smalldepth[i]/calls[i];
			if (percent == 0) {
	      printf("  %7ld      -             %3ld          %3d         %3d\n", calls[i], maxdepth[i], avdepth, smdepth);
			}
			else {
	      printf("  %7ld     %2ld             %3ld          %3d         %3d\n", calls[i], percent, maxdepth[i], avdepth, smdepth);
			}

      if (i%max_structs + 1 == max_structs) {
			  short totaldepth = totalCalls == 0 ? 0 : round(float(totalDepth)/float(totalCalls));
		    printf("TOTAL                       ");
	      printf("  %7ld    100             %3ld          %3d    %ld ms\n", totalCalls, totalMaxdepth, totaldepth, microsecs[i/max_structs]/1000);
      }

    }
    printf("------------------------------------------------------------------------------------------\n");
    printf("TOTAL time = %ld microsecs (= %ld ms)\n", totalMicrosecs, totalMicrosecs/1000);
    printf("------------------------------------------------------------------------------------------\n\n\n");
  }

	if (resetCounters) {
		resetCounters = PR_FALSE;
		totalMicrosecs = 0;
		for (short i = 0; i < logCallType_Max; i ++) {
		  microsecs[i] = 0L;
		}
		for (short i = 0; i < (max_structs*logCallType_Max); i ++) {
		  calls[i] = 0L;
		  depth[i] = 0L;
		  maxdepth[i] = 0L;
		  smalldepth[i] = 0L;
		}
	}

  short index = aSID - 1;
  index += max_structs * aLogCallType;

  calls[index]++;

  unsigned long curdepth = 0;
  nsCOMPtr<nsIStyleContext> childContext;
  nsCOMPtr<nsIStyleContext> parentContext;
  childContext  = aStyleContext;
  parentContext = getter_AddRefs(childContext->GetParent());
  while (parentContext != nsnull) {
    curdepth++;
    parentContext = getter_AddRefs(childContext->GetParent());
    if (parentContext == childContext) {
      break;
    }
    childContext = parentContext;
  }
	depth[index] += curdepth;
  if (maxdepth[index] < curdepth) {
    maxdepth[index] = curdepth;
  }
  if (curdepth <= small_depth_threshold) {
  	smalldepth[index]++;
  }

  static short inCount = 0;
  static short outCount = 0;
  if (inCount++ % 1000 == 0) {
    switch (outCount++) {
      case 0:  printf("still logging"); 		 break;
      case 20: printf("\n"); outCount = 0;	 break;
      default: printf(".");  fflush(stdout); break;
    }
  }
  ::Microseconds(&startMicrosecs);
}
#endif // LOG_GET_STYLE_DATA_CALLS

//=========================================================================================================

#ifdef LOG_WRITE_STYLE_DATA_CALLS
static void LogWriteMutableStyleDataCall(nsStyleStructID aSID, nsStyleStruct* aStyleStruct, StyleContextImpl* aStyleContext)
{
#define max_structs eStyleStruct_Max
  static unsigned long calls[max_structs];
  static unsigned long unchanged[max_structs];

	static PRBool resetCounters = PR_TRUE;
  if (IsTimeToDumpDebugData()) {
    resetCounters = PR_TRUE;

	  printf("------------------------------------------------------------------------------------------\n");
	  printf("WriteMutableStyleData\n");
	  printf("------------------------------------------------------------------------------------------\n");
	  printf("                                calls   unchanged%c\n", '%');
	  for (short i = 0; i < max_structs; i ++) {
	    switch (i+1) {
		    case eStyleStruct_Font:      			printf("eStyleStruct_Font           "); break;
		    case eStyleStruct_Color:      		printf("eStyleStruct_Color          "); break;
		    case eStyleStruct_List:      			printf("eStyleStruct_List           "); break;
		    case eStyleStruct_Position:      	printf("eStyleStruct_Position       "); break;
		    case eStyleStruct_Text:      			printf("eStyleStruct_Text           "); break;
		    case eStyleStruct_Display:      	printf("eStyleStruct_Display        "); break;
		    case eStyleStruct_Table:      		printf("eStyleStruct_Table          "); break;
		    case eStyleStruct_Content:      	printf("eStyleStruct_Content        "); break;
		    case eStyleStruct_UserInterface:  printf("eStyleStruct_UserInterface  "); break;
		    case eStyleStruct_Print: 					printf("eStyleStruct_Print          "); break;
		    case eStyleStruct_Margin: 				printf("eStyleStruct_Margin         "); break;
		    case eStyleStruct_Padding: 				printf("eStyleStruct_Padding        "); break;
		    case eStyleStruct_Border: 				printf("eStyleStruct_Border         "); break;
		    case eStyleStruct_Outline: 				printf("eStyleStruct_Outline        "); break;
#ifdef INCLUDE_XUL
		    case eStyleStruct_XUL: 		    		printf("eStyleStruct_XUL            "); break;
#endif
        //#insert new style structs here#
	    }
			short percent = 100*unchanged[i]/calls[i];
			if (percent == 0) {
	      printf("  %7ld      -\n", calls[i]);
			}
			else {
	      printf("  %7ld     %2ld\n", calls[i], percent);
			}
	  }
	  printf("------------------------------------------------------------------------------------------\n\n\n");
  }

  if (resetCounters) {
    resetCounters = PR_FALSE;
    for (short i=0; i<max_structs; i++) {
      calls[i] = 0;
      unchanged[i] = 0;
    }
  }

  calls[aSID-1] ++;
  switch (aSID) {
    case eStyleStruct_Font:
      if (aStyleContext->GETSCDATA(Font)->CalcDifference(*(nsStyleFont*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Color:
      if (aStyleContext->GETSCDATA(Color)->CalcDifference(*(nsStyleColor*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_List:
      if (aStyleContext->GETSCDATA(List)->CalcDifference(*(nsStyleList*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Position:
      if (aStyleContext->GETSCDATA(Position)->CalcDifference(*(nsStylePosition*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Text:
      if (aStyleContext->GETSCDATA(Text)->CalcDifference(*(nsStyleText*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Display:
      if (aStyleContext->GETSCDATA(Display)->CalcDifference(*(nsStyleDisplay*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Table:
      if (aStyleContext->GETSCDATA(Table)->CalcDifference(*(nsStyleTable*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Content:
      if (aStyleContext->GETSCDATA(Content)->CalcDifference(*(nsStyleContent*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_UserInterface:
      if (aStyleContext->GETSCDATA(UserInterface)->CalcDifference(*(nsStyleUserInterface*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Print:
      if (aStyleContext->GETSCDATA(Print)->CalcDifference(*(nsStylePrint*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Margin:
      if (aStyleContext->GETSCDATA(Margin)->CalcDifference(*(nsStyleMargin*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Padding:
      if (aStyleContext->GETSCDATA(Padding)->CalcDifference(*(nsStylePadding*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Border:
      if (aStyleContext->GETSCDATA(Border)->CalcDifference(*(nsStyleBorder*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
    case eStyleStruct_Outline:
      if (aStyleContext->GETSCDATA(Outline)->CalcDifference(*(nsStyleOutline*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
#ifdef INCLUDE_XUL
		case eStyleStruct_XUL:
      if (aStyleContext->GETSCDATA(XUL)->CalcDifference(*(nsStyleXUL*)aStyleStruct) == NS_STYLE_HINT_NONE)
        unchanged[aSID-1]++;
    	break;
#endif
    //#insert new style structs here#
    default:
      NS_ERROR("Invalid style struct id");
      break;
  }
}
#endif // LOG_WRITE_STYLE_DATA_CALLS
