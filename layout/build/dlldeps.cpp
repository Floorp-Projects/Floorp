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

// Force references to all of the symbols that we want exported from
// the dll that are located in the .lib files we link with

#include "nsString.h"
#include "nsIPresShell.h"
#include "nsIPrintContext.h"
#include "nsIPresContext.h"
#include "nsIStyleSet.h"
#include "nsIDocument.h"
#include "nsHTMLParts.h"
#include "nsINameSpaceManager.h"

void XXXNeverCalled()
{
  nsIPresShell* ps;
  NS_NewPresShell(&ps);
  nsIPresContext* cx;
  nsIPrintContext *px;
  NS_NewGalleyContext(&cx);
  NS_NewPrintPreviewContext(&cx);
  NS_NewPrintContext(&px);
  nsIFrame* f;
  NS_NewTextFrame(ps, &f);
  NS_NewInlineFrame(ps, &f);
  NS_NewBRFrame(ps, &f);
  NS_NewWBRFrame(ps, &f);
  NS_NewHRFrame(ps, &f);
  NS_NewObjectFrame(ps, &f);
  NS_NewSpacerFrame(ps, &f);
  NS_NewHTMLFramesetFrame(ps, &f);
  NS_NewCanvasFrame(ps, &f);
  NS_NewScrollFrame(ps, &f);
  NS_NewSimplePageSequenceFrame(ps, &f);
}
