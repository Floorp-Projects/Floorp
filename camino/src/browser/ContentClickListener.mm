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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@mozilla.org> (Original Author)
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


#include "nsCOMPtr.h"

#include "nsEmbedAPI.h"
#include "nsString.h"
#include "nsUnicharUtils.h"

#include "nsIDOMElement.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventTarget.h"

// Common helper routines (also used by the context menu code)
#include "GeckoUtils.h"

#import "NSString+Utils.h"
#import "PreferenceManager.h"
#import "CHBrowserView.h"

#import "ContentClickListener.h"

NS_IMPL_ISUPPORTS2(ContentClickListener, nsIDOMMouseListener, nsIDOMEventListener)

ContentClickListener::ContentClickListener(id aBrowserController)
:mBrowserController(aBrowserController)
{
}

ContentClickListener::~ContentClickListener()
{

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
  GeckoUtils::GetEnclosingLinkElementAndHref(content, getter_AddRefs(linkContent), href);
  
  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (!linkContent || href.IsEmpty())
    return NS_OK;
    
  PRUint16 button;
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aEvent));
  mouseEvent->GetButton(&button);

  PRBool metaKey, shiftKey, altKey;
  mouseEvent->GetMetaKey(&metaKey);
  mouseEvent->GetShiftKey(&shiftKey);
  mouseEvent->GetAltKey(&altKey);

  NSString* hrefStr = [NSString stringWith_nsAString:href];

  if ((metaKey && button == 0) || button == 1) {
    NSString* referrer = [[[mBrowserController getBrowserWrapper] getBrowserView] getFocusedURLString];

    NSRange firstColon = [hrefStr rangeOfString:@":"];
    NSString* hrefScheme;
    if (firstColon.location != NSNotFound)
      hrefScheme = [hrefStr substringToIndex:firstColon.location];
    else
      hrefScheme = @"file"; // implicitly file:// if no colon is found

    // The Command key is down or we got a middle-click.
    // Open the link in a new window or tab if it's an internally handled, non-Javascript link.
    if (![hrefScheme isEqualToString:@"javascript"] && GeckoUtils::isProtocolInternal([hrefScheme UTF8String])) {
      BOOL useTab           = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL];
      BOOL loadInBackground = [BrowserWindowController shouldLoadInBackground:nil];

      if (useTab)
        [mBrowserController openNewTabWithURL:hrefStr referrer:referrer loadInBackground:loadInBackground allowPopups:NO setJumpback:YES];
      else
        [mBrowserController openNewWindowWithURL:hrefStr referrer:referrer loadInBackground:loadInBackground allowPopups:NO];
    }
    else { // It's an external protocol or a "javascript:" URL, so just open the link.
      [mBrowserController loadURL:hrefStr referrer:referrer focusContent:YES allowPopups:NO];
    }
  }
  else if (altKey) {
    // The user wants to save this link.
    nsAutoString text;
    GeckoUtils::GatherTextUnder(content, text);

    [mBrowserController saveURL:nil url:hrefStr suggestedFilename:[NSString stringWith_nsAString:text]];
  }

  return NS_OK;
}
