//
//  PreferenceManager.h
//  Chimera
//
//  Created by William Dell Wisner on Wed Apr 17 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Carbon/Carbon.h>

@interface PreferenceManager : NSObject {
    NSUserDefaults *defaults;
    ICInstance internetConfig;
}

- (id) init;
- (void) dealloc;
- (BOOL) initInternetConfig;
- (BOOL) initMozillaPrefs;
- (void) syncMozillaPrefs;
// - (BOOL) getICBoolPref:(ConstStr255Param) prefKey;
- (NSString *) getICStringPref:(ConstStr255Param) prefKey;
- (NSString *) homePage;

@end
