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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
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

#import <Cocoa/Cocoa.h>

#import "RemoteDataProvider.h"

extern NSString* SiteIconLoadNotificationName;
extern NSString* SiteIconLoadImageKey;
extern NSString* SiteIconLoadURIKey;
extern NSString* SiteIconLoadUserDataKey;

class NeckoCacheHelper;

@interface SiteIconProvider : NSObject<RemoteLoadListener>
{
  NeckoCacheHelper* mMissedIconsCacheHelper;
}

+ (SiteIconProvider*)sharedFavoriteIconProvider;

+ (NSString*)faviconLocationStringFromURI:(NSString*)inURI;

// Start a favicon.ico load for the given URI, which can be any URI.
// The caller will get a 'SiteIconLoadNotificationName' notification
// when the load is done, with the image at the 'SiteIconLoadImageKey' key
// in the notifcation userInfo. The caller will have had to register with the
// NSNotifcationCenter in order to receive this notifcation. The notification
// is dispatched with 'sender' as the object.
// This method returns YES if the uri request was dispatched (i.e. if we know
// that we've looked for, and failed to find, this icon before). If it returns
// YES, then the 'SiteIconLoadNotificationName' notification will be sent out.
// userData is not retained.
- (BOOL)loadFavoriteIcon:(id)sender forURI:(NSString *)inURI withUserData:(id)userData allowNetwork:(BOOL)inAllowNetwork;

@end
