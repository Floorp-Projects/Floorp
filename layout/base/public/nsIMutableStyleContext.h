/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIMutableStyleContext_h___
#define nsIMutableStyleContext_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsStyleStruct.h"


#define NS_IMUTABLESTYLECONTEXT_IID   \
 { 0x53cbb100, 0x8340, 0x11d3, \
   {0xba, 0x05, 0x00, 0x10, 0x83, 0x02, 0x3c, 0x2b} }


class nsIMutableStyleContext : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMUTABLESTYLECONTEXT_IID; return iid; }

  virtual nsIStyleContext*  GetParent(void) const = 0;

  // Fill a style struct with data
  NS_IMETHOD GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const = 0;

  // Get a pointer on the style data
  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID) = 0;

  // Modify the style data
  //
  //   Calls to ReadMutableStyleData() and WriteMutableStyleData() must be balanced.
  //   To enforce this, they are not made public.  You must use the helper classes below
  //   where the constructors and destructors call Read() and Write() for you.
  //
  //   For instance, to change the opacity, you should do:
  //
  //            nsMutableStyleColor color(myStyleContext);
  //            color->mOpacity = 0.5f;
  //
  //   In some cases, you may want to put the block above between brackets {...}
  //   so that the |nsMutableStyleColor| destructor can be called and your changes
  //   written into the style context.
  //
  virtual void ReadMutableStyleData(nsStyleStructID aSID, nsStyleStruct** aStyleStructPtr) = 0;
  virtual nsresult WriteMutableStyleData(nsStyleStructID aSID, nsStyleStruct* aStyleStruct) = 0;
};

template <class T, nsStyleStructID SID>
class basic_nsAutoMutableStyle
{
protected:
  T*     mStyleStruct;
  void*  mContext;
  PRBool mMutable;

public:
  basic_nsAutoMutableStyle(nsIMutableStyleContext* aContext)
    : mContext(aContext), mMutable(PR_TRUE)
  {
    if (aContext)
      aContext->ReadMutableStyleData(SID, &(nsStyleStruct*)mStyleStruct);
  }

  basic_nsAutoMutableStyle(nsIStyleContext* aContext)
    : mContext(aContext), mMutable(PR_FALSE)
  {
    if (aContext)
      aContext->ReadMutableStyleData(SID, &(nsStyleStruct*)mStyleStruct);
  }

  ~basic_nsAutoMutableStyle()
  {
    if (mContext) {
      if (mMutable) {
        nsIMutableStyleContext* context =
          NS_STATIC_CAST(nsIMutableStyleContext*, mContext);

        context->WriteMutableStyleData(SID, mStyleStruct);
      }
      else {
        nsIStyleContext* context =
          NS_STATIC_CAST(nsIStyleContext*, mContext);

        context->WriteMutableStyleData(SID, mStyleStruct);
      }
    }
  }

  T* get() { return mStyleStruct; }
  T* operator->() { return get(); }
  T& operator*() { return *get(); }
 };

// Helper classes to modify the style data
typedef basic_nsAutoMutableStyle<nsStyleFont,          eStyleStruct_Font>          nsMutableStyleFont;
typedef basic_nsAutoMutableStyle<nsStyleColor,         eStyleStruct_Color>         nsMutableStyleColor;
typedef basic_nsAutoMutableStyle<nsStyleList,          eStyleStruct_List>          nsMutableStyleList;
typedef basic_nsAutoMutableStyle<nsStylePosition,      eStyleStruct_Position>      nsMutableStylePosition;
typedef basic_nsAutoMutableStyle<nsStyleText,          eStyleStruct_Text>          nsMutableStyleText;
typedef basic_nsAutoMutableStyle<nsStyleDisplay,       eStyleStruct_Display>       nsMutableStyleDisplay;
typedef basic_nsAutoMutableStyle<nsStyleTable,         eStyleStruct_Table>         nsMutableStyleTable;
typedef basic_nsAutoMutableStyle<nsStyleContent,       eStyleStruct_Content>       nsMutableStyleContent;
typedef basic_nsAutoMutableStyle<nsStyleUserInterface, eStyleStruct_UserInterface> nsMutableStyleUserInterface;
typedef basic_nsAutoMutableStyle<nsStylePrint,         eStyleStruct_Print>         nsMutableStylePrint;
typedef basic_nsAutoMutableStyle<nsStyleMargin,        eStyleStruct_Margin>        nsMutableStyleMargin;
typedef basic_nsAutoMutableStyle<nsStylePadding,       eStyleStruct_Padding>       nsMutableStylePadding;
typedef basic_nsAutoMutableStyle<nsStyleBorder,        eStyleStruct_Border>        nsMutableStyleBorder;
typedef basic_nsAutoMutableStyle<nsStyleOutline,       eStyleStruct_Outline>       nsMutableStyleOutline;
#ifdef INCLUDE_XUL
typedef basic_nsAutoMutableStyle<nsStyleXUL,           eStyleStruct_XUL>           nsMutableStyleXUL;
#endif

#endif /* nsIMutableStyleContext_h___ */
