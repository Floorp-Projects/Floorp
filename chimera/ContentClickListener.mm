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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "ContentClickListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMElement.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIPrefBranch.h"
#include "nsIDOMMouseEvent.h"
#include "nsEmbedAPI.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsIURI.h"
#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"

// Common helper routines (also used by the context menu code)
#include "CHGeckoUtils.h"

#import "CHBrowserView.h"

static void FindOptionWithContentID(nsIDOMHTMLSelectElement* aSel, PRUint32 aID, nsIDOMHTMLOptionElement** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsIDOMHTMLCollection> options;
  aSel->GetOptions(getter_AddRefs(options));
  PRUint32 count;
  options->GetLength(&count);
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> node;
    options->Item(i, getter_AddRefs(node));
    PRUint32 contentID;
    nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    content->GetContentID(&contentID);
    if (contentID == aID) {
      nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));
      *aResult = option;
      NS_ADDREF(*aResult);
      break;
    }
  }
}

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
  int tag = [aSender tag];
  nsCOMPtr<nsIDOMHTMLOptionElement> optionElt;
  FindOptionWithContentID(mSelectElt, tag, getter_AddRefs(optionElt));
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

NS_IMPL_ISUPPORTS2(ContentClickListener, nsIDOMMouseListener, nsIDOMEventListener);

ContentClickListener::ContentClickListener(id aBrowserController)
:mBrowserController(aBrowserController)
{
  NS_INIT_ISUPPORTS();
}

ContentClickListener::~ContentClickListener()
{

}

NS_IMETHODIMP
ContentClickListener::MouseDown(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  if (!target)
    return NS_OK;
  nsCOMPtr<nsIDOMHTMLSelectElement> sel(do_QueryInterface(target));
  if (sel) {
    NSMenu* menu = [[NSMenu alloc] init]; // Retain the menu.
    nsCOMPtr<nsIDOMHTMLCollection> options;
    sel->GetOptions(getter_AddRefs(options));
    PRUint32 count;
    options->GetLength(&count);
    PRInt32 selIndex = 0;
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIDOMNode> node;
      options->Item(i, getter_AddRefs(node));
      PRUint32 contentID;
      nsCOMPtr<nsIContent> content(do_QueryInterface(node));
      content->GetContentID(&contentID);
      nsAutoString text;
      nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));
      option->GetLabel(text);
      if (text.IsEmpty())
        option->GetText(text);
      NSString* title = [NSString stringWithCharacters: text.get() length: nsCRT::strlen(text.get())];
      NSMenuItem* menuItem = [[[NSMenuItem alloc] initWithTitle: title action: NULL keyEquivalent: @""] autorelease];
      [menu addItem: menuItem];
      [menuItem setTag: contentID];
      PRBool selected;
      option->GetSelected(&selected);
      if (selected) {
        [menuItem setState: NSOnState];
        selIndex = i;
      }
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

    nsCOMPtr<nsIDOMWindow> window = getter_AddRefs([[[mBrowserController getBrowserWrapper] getBrowserView] getContentWindow]);
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
      
    [NSMenu popUpContextMenu: menu withEvent: mouseEvent forView: [[mBrowserController window] contentView]];
    
  }
  return NS_OK;
}

NS_IMETHODIMP
ContentClickListener::MouseClick(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  if (!target)
    return NS_OK;
  nsCOMPtr<nsIDOMNode> content(do_QueryInterface(target));

  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  CHGeckoUtils::GetEnclosingLinkElementAndHref(content, getter_AddRefs(linkContent), href);
  
  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (!linkContent || href.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  if (!pref)
    return NS_OK; // Something bad happened if we can't get prefs.
    
  PRUint16 button;
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aEvent));
  mouseEvent->GetButton(&button);

  PRBool metaKey, shiftKey, altKey;
  mouseEvent->GetMetaKey(&metaKey);
  mouseEvent->GetShiftKey(&shiftKey);
  mouseEvent->GetAltKey(&altKey);

  NSString* hrefStr = [NSString stringWithCharacters: href.get() length: href.Length()];
  
  // Hack to determine specific protocols handled by Chimera in the frontend
  // until I can determine why the general unknown protocol handler handoff
  // between Necko and uriloader isn't happening.
  // NS_NewURI returns nil for the uri if we don't have a handler for the protocol
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), href);
  if (uri) {
    // Fall through and do whatever we'd normally do with this kind of URL
  } else {
    NSString* escapedString = (NSString*) CFURLCreateStringByAddingPercentEscapes(NULL, (CFStringRef) hrefStr, NULL, NULL, kCFStringEncodingUTF8);
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: escapedString]];
  }

  if ((metaKey && button == 0) || button == 1) {
    // The command key is down or we got a middle click.  Open the link in a new window or tab.
    PRBool useTab;
    pref->GetBoolPref("browser.tabs.opentabfor.middleclick", &useTab);
    PRBool loadInBackground;
    pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);
    NSString* referrer = [[[mBrowserController getBrowserWrapper] getBrowserView] getFocusedURLString];
    
    if (shiftKey)
      loadInBackground = !loadInBackground;
    if (useTab)
      [mBrowserController openNewTabWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground];
    else
      [mBrowserController openNewWindowWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground];
  }
  else if (altKey) {
    // The user wants to save this link.
    nsAutoString text;
    CHGeckoUtils::GatherTextUnder(content, text);

    [mBrowserController saveURL: nil filterList: nil
              url: hrefStr suggestedFilename: [NSString stringWithCharacters: text.get()
                                                                      length: nsCRT::strlen(text.get())]];
  }

  return NS_OK;
}
