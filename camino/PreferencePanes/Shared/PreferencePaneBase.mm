//
//  PreferencePaneBase.mm
//  Chimera
//
//  Created by Simon Fraser on Thu Jul 11 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "PreferencePaneBase.h"

#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPref.h"
#include "nsIMemory.h"

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
