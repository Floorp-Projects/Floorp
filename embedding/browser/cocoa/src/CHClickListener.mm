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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2002 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  David Hyatt <hyatt@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#import "NSString+Utils.h"
#import "CHBrowserView.h"

#include "nsCOMPtr.h"
#include "CHClickListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMMouseEvent.h"
#include "nsEmbedAPI.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsString.h"


@interface CHOptionSelector : NSObject
{
  nsIDOMHTMLSelectElement* mSelectElt;
}

-(id)initWithSelect:(nsIDOMHTMLSelectElement*)aSel;
-(IBAction)selectOption:(id)aSender;

@end

@implementation CHOptionSelector

-(id)initWithSelect:(nsIDOMHTMLSelectElement*)aSel
{
  if ( (self = [super init]) ) {
    mSelectElt = aSel;
  }
  return self;
}

-(IBAction)selectOption:(id)aSender
{
  nsIDOMHTMLOptionElement* optionElt = (nsIDOMHTMLOptionElement*) [aSender tag];
  optionElt->SetSelected(PR_TRUE);
  [self autorelease]; // Free up ourselves.
  [[aSender menu] autorelease]; // Free up the menu.

  // Fire a DOM event for the title change.
  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<nsIDOMDocument> domDocument;
  mSelectElt->GetOwnerDocument(getter_AddRefs(domDocument));
  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(domDocument));
  
  docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (event) {
    event->InitEvent(NS_LITERAL_STRING("change"), PR_TRUE, PR_TRUE);
    PRBool noDefault;
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mSelectElt));
    target->DispatchEvent(event, &noDefault);
  }  
}

@end


NS_IMPL_ISUPPORTS2(CHClickListener, nsIDOMMouseListener, nsIDOMEventListener)

CHClickListener::CHClickListener()
{
}

CHClickListener::~CHClickListener()
{
}

NS_IMETHODIMP
CHClickListener::MouseDown(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  if (!target)
    return NS_OK;
  nsCOMPtr<nsIDOMHTMLSelectElement> sel(do_QueryInterface(target));
  if (sel) {
    PRInt32 size = 0;
    sel->GetSize(&size);
    PRBool multiple = PR_FALSE;
    sel->GetMultiple(&multiple);
    if(size > 1 || multiple)
      return NS_OK;
      
    NSMenu* menu = [[NSMenu alloc] init]; // Retain the menu.

    // We'll set the disabled state as the options are created, so disable
    // auto-enabling via NSMenuValidation.
    [menu setAutoenablesItems: NO];

    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    sel->GetOptions(getter_AddRefs(options));
    PRUint32 count;
    options->GetLength(&count);
    PRInt32 selIndex = 0;
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIDOMNode> node;
      options->Item(i, getter_AddRefs(node));
      nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));
      nsAutoString text;
      option->GetLabel(text);
      if (text.IsEmpty())
        option->GetText(text);
      NSString* title = [[NSString stringWith_nsAString: text] stringByTruncatingTo:75 at:kTruncateAtMiddle];
      NSMenuItem* menuItem = [[[NSMenuItem alloc] initWithTitle: title action: NULL keyEquivalent: @""] autorelease];
      [menu addItem: menuItem];
      [menuItem setTag: (int)option.get()];
      PRBool selected;
      option->GetSelected(&selected);
      if (selected) {
        [menuItem setState: NSOnState];
        selIndex = i;
      }
      PRBool disabled;
      option->GetDisabled(&disabled);
      if (disabled)
        [menuItem setEnabled: NO];
      CHOptionSelector* optSelector = [[CHOptionSelector alloc] initWithSelect: sel];
      [menuItem setTarget: optSelector];
      [menuItem setAction: @selector(selectOption:)];
    }

    nsCOMPtr<nsIDOMNSHTMLElement> nsSel(do_QueryInterface(sel));
    PRInt32 left, top, height;
    PRInt32 clientX, clientY;
    nsSel->GetOffsetLeft(&left);
    nsSel->GetOffsetTop(&top);
    nsSel->GetOffsetHeight(&height);

    nsCOMPtr<nsIDOMElement> currOffsetParent;
    nsSel->GetOffsetParent(getter_AddRefs(currOffsetParent));
    while (currOffsetParent) {
      nsCOMPtr<nsIDOMNSHTMLElement> currNS(do_QueryInterface(currOffsetParent));
      PRInt32 currLeft, currTop;
      currNS->GetOffsetLeft(&currLeft);
      currNS->GetOffsetTop(&currTop);
      left += currLeft;
      top += currTop;
      currNS->GetOffsetParent(getter_AddRefs(currOffsetParent));
    }
    
    nsCOMPtr<nsIDOMMouseEvent> msEvent(do_QueryInterface(aEvent));
    msEvent->GetClientX(&clientX);
    msEvent->GetClientY(&clientY);

    PRInt32 xDelta = clientX - left;
    PRInt32 yDelta = top + height - clientY;

    nsCOMPtr<nsIContent> selContent = do_QueryInterface(sel);
    nsCOMPtr<nsIDocument> doc = selContent->GetDocument();

    // I'm going to assume that if we got a mousedown for a content node,
    // it's actually in a document.

    nsCOMPtr<nsIScriptGlobalObject> sgo;
    doc->GetScriptGlobalObject(getter_AddRefs(sgo));
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(sgo);
    PRInt32 scrollX, scrollY;
    window->GetScrollX(&scrollX);
    window->GetScrollY(&scrollY);
    xDelta += scrollX; // Normal direction.
    yDelta -= scrollY; // Remember, y is flipped.
    
#define XMENUOFFSET 20
#define MENUHEIGHT 20
    
    xDelta += XMENUOFFSET;
    yDelta -= MENUHEIGHT*(selIndex+1);
    
    NSEvent* event = [NSApp currentEvent];
    NSPoint point = [event locationInWindow];
    point.x -= xDelta;
    point.y -= yDelta;

    NSEvent* mouseEvent = [NSEvent mouseEventWithType: NSLeftMouseDown location: point
                                        modifierFlags: 0 timestamp: [event timestamp]
                                         windowNumber: [event windowNumber] context: [event context]
                                          eventNumber: [event eventNumber] clickCount: [event clickCount] 																						 pressure: [event pressure]];
    [NSMenu popUpContextMenu: menu withEvent: mouseEvent forView: [[event window] contentView]];
  }
  return NS_OK;
}
