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

#import <Foundation/Foundation.h>


extern NSString* const RemoteDataLoadRequestNotificationName;
extern NSString* const RemoteDataLoadRequestURIKey;
extern NSString* const RemoteDataLoadRequestDataKey;
extern NSString* const RemoteDataLoadRequestUserDataKey;
extern NSString* const RemoteDataLoadRequestResultKey;

// RemoteDataProvider is a class that can be used to do asynchronous loads
// from URIs using necko, and passing back the result of the load to a
// callback in NSData.
//
// Clients can either implement the RemoteLoadListener protocol and call
// loadURI directly, or they can register with the [NSNotification defaultCenter]
// for 'RemoteDataLoadRequestNotificationName' notifications, and catch all loads
// that happen that way.

@protocol RemoteLoadListener
// called when the load completes, or fails. If the status code is a failure code,
// data may be nil.
- (void)doneRemoteLoad:(NSString*)inURI forTarget:(id)target withUserData:(id)userData data:(NSData*)data status:(nsresult)status usingNetwork:(BOOL)usingNetwork;
@end


class RemoteURILoadManager;

@interface RemoteDataProvider : NSObject<RemoteLoadListener>
{
  RemoteURILoadManager* mLoadManager;
}

+ (RemoteDataProvider*)sharedRemoteDataProvider;

// generic method. You can load any URI asynchronously with this selector,
// and the listener will get the contents of the URI in an NSData.
// If allowNetworking is NO, then this method will just check the cache,
// and not go to the network
// This method will return YES if the request was dispatched, or NO otherwise.
- (BOOL)loadURI:(NSString*)inURI forTarget:(id)target withListener:(id<RemoteLoadListener>)inListener
          withUserData:(id)userData allowNetworking:(BOOL)inNetworkOK;

// specific request to load a remote file. The sender (or any other object), if
// registered with the notification center, will receive a notification when
// the load completes. The 'target' becomes the 'object' of the notification.
// The notification name is given by NSString* RemoteDataLoadRequestNotificationName above.
// If allowNetworking is NO, then this method will just check the cache,
// and not go to the network
// This method will return YES if the request was dispatched, or NO otherwise.
- (BOOL)postURILoadRequest:(NSString*)inURI forTarget:(id)target
          withUserData:(id)userData allowNetworking:(BOOL)inNetworkOK;

// cancel all outstanding requests
- (void)cancelOutstandingRequests;

@end
