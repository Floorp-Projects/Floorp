//
//  PreferencePaneBase.h
//  Chimera
//
//  Created by Simon Fraser on Thu Jul 11 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//
#import <PreferencePanes/PreferencePanes.h>

class nsIPref;

@interface PreferencePaneBase : NSPreferencePane 
{
  nsIPref* 				mPrefService;					// strong, but can't use a comptr here
}

- (NSString*)getStringPref: (const char*)prefName withSuccess:(BOOL*)outSuccess;
- (NSColor*)getColorPref: (const char*)prefName withSuccess:(BOOL*)outSuccess;
- (BOOL)getBooleanPref: (const char*)prefName withSuccess:(BOOL*)outSuccess;
- (int)getIntPref: (const char*)prefName withSuccess:(BOOL*)outSuccess;

- (void)setPref: (const char*)prefName toString:(NSString*)value;
- (void)setPref: (const char*)prefName toColor:(NSColor*)value;
- (void)setPref: (const char*)prefName toBoolean:(BOOL)value;
- (void)setPref: (const char*)prefName toInt:(int)value;

- (void)clearPref: (const char*)prefName;

- (NSString*)getLocalizedString:(NSString*)key;

@end
