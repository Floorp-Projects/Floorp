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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser
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

#import "PreferencePaneBase.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"

@implementation PreferencePaneBase

- (void) dealloc
{
  NS_IF_RELEASE(mPrefService);
  [super dealloc];
}

- (id) initWithBundle:(NSBundle *) bundle
{
  self = [super initWithBundle:bundle] ;
  
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, pref panel left uninitialized");
  mPrefService = prefService.get();
  NS_IF_ADDREF(mPrefService);
    
  return self;
}

#pragma mark -

- (NSString*)getStringPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  NSString *prefValue = @"";
  
  char *buf = nsnull;
  nsresult rv = mPrefService->GetCharPref(prefName, &buf);
  if (NS_SUCCEEDED(rv) && buf) {
    // prefs are UTF-8
    prefValue = [NSString stringWithUTF8String:buf];
    free(buf);
    if (outSuccess) *outSuccess = YES;
  } else {
    if (outSuccess) *outSuccess = NO;
  }
  
  return prefValue;
}

- (NSColor*)getColorPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  // colors are stored in HTML-like #FFFFFF strings
  NSString* colorString = [self getStringPref:prefName withSuccess:outSuccess];
  NSColor* 	returnColor = [NSColor blackColor];

  if ([colorString hasPrefix:@"#"] && [colorString length] == 7)
  {
    unsigned int redInt, greenInt, blueInt;
    sscanf([colorString cString], "#%02x%02x%02x", &redInt, &greenInt, &blueInt);
    
    float redFloat 		= ((float)redInt / 255.0);
    float	greenFloat 	= ((float)greenInt / 255.0);
    float blueFloat 	= ((float)blueInt / 255.0);
    
    returnColor = [NSColor colorWithCalibratedRed:redFloat green:greenFloat blue:blueFloat alpha:1.0f];
  }
  else
  {
    *outSuccess = NO;
  }

  return returnColor;
}

- (BOOL)getBooleanPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  PRBool boolPref = PR_FALSE;
  nsresult rv = mPrefService->GetBoolPref(prefName, &boolPref);
  if (outSuccess)
    *outSuccess = NS_SUCCEEDED(rv);

  return boolPref ? YES : NO;
}

- (int)getIntPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  PRInt32 intPref = 0;
  nsresult rv = mPrefService->GetIntPref(prefName, &intPref);
  if (outSuccess)
    *outSuccess = NS_SUCCEEDED(rv);
  return intPref;
}

- (void)setPref: (const char*)prefName toString:(NSString*)value
{
  // prefs are UTF-8
  if (mPrefService) {
    mPrefService->SetCharPref(prefName, [value UTF8String]);
  }
}

- (void)setPref: (const char*)prefName toColor:(NSColor*)value
{
  // make sure we have a color in the RGB colorspace
  NSColor*	rgbColor = [value colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  
  int	redInt 		= (int)([rgbColor redComponent] * 255.0);
  int greenInt	= (int)([rgbColor greenComponent] * 255.0);
  int blueInt		= (int)([rgbColor blueComponent] * 255.0);

  NSString* colorString = [NSString stringWithFormat:@"#%02x%02x%02x", redInt, greenInt, blueInt];
  [self setPref:prefName toString:colorString];
}

- (void)setPref: (const char*)prefName toBoolean:(BOOL)value
{
  if (mPrefService) {
    mPrefService->SetBoolPref(prefName, value ? PR_TRUE : PR_FALSE);
  }
}

- (void)setPref: (const char*)prefName toInt:(int)value
{
  if (mPrefService) {
    PRInt32 prefValue = value;
    mPrefService->SetIntPref(prefName, prefValue);
  }
}

- (void)clearPref: (const char*)prefName
{
  if (mPrefService) {
    mPrefService->ClearUserPref(prefName);
  }
}

- (NSString*)getLocalizedString:(NSString*)key
{
  return NSLocalizedStringFromTableInBundle(key, nil, [NSBundle bundleForClass:[self class]], @"");
}

@end
