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
*    David Haas <haasd@cae.wisc.edu>
* 
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

// superclass for Bookmark & BookmarkFolder.
// Basically here to aid in scripting support.

#import <Appkit/Appkit.h>

@interface BookmarkItem : NSObject <NSCopying>
{
  id  mParent;	//subclasses will use a BookmarkFolder
  NSString* mTitle;       
  NSString* mDescription;
  NSString* mKeyword; 
  NSString* mUUID;
  NSImage* mIcon;
  BOOL mAccumulateItemChangeUpdates;
}

// Setters/Getters
-(id) parent;
-(NSString *) title;
-(NSString *) description;
-(NSString *) keyword;
-(NSImage *) icon;
-(NSString *) UUID;

-(void)	setParent:(id)aParent;
-(void) setTitle:(NSString *)aString;
-(void) setDescription:(NSString *)aString;
-(void) setKeyword:(NSString *)aKeyword;
-(void) setIcon:(NSImage *)aIcon;
-(void) setUUID:(NSString *)aUUID;

// Status checks
-(BOOL) isChildOfItem:(BookmarkItem *)anItem;

// Notificaiton of Change
+(void) setSuppressAllUpdateNotifications:(BOOL)suppressUpdates;
+(BOOL) allowNotifications;
-(void) setAccumulateUpdateNotifications:(BOOL)suppressUpdates;
-(void) itemUpdatedNote; //right now, just on title & icon - for BookmarkButton & BookmarkMenu notes

// Methods called on startup for both bookmark & folder
-(void) refreshIcon;

  // for reading/writing to disk - unimplemented in BookmarkItem.
-(BOOL) readNativeDictionary:(NSDictionary *)aDict;
-(BOOL) readSafariDictionary:(NSDictionary *)aDict;
-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef;

-(NSDictionary *)writeNativeDictionary;
-(NSDictionary *)writeSafariDictionary;
-(NSString *)writeHTML:(unsigned)aPad;

@end

// Bunch of Keys for reading/writing dictionaries.

// Safari & Camino plist keys
extern NSString *BMTitleKey;
extern NSString *BMChildrenKey;

// Camino plist keys
extern NSString *BMFolderDescKey;
extern NSString *BMFolderTypeKey;
extern NSString *BMFolderKeywordKey;
extern NSString *BMDescKey;
extern NSString *BMStatusKey;
extern NSString *BMURLKey;
extern NSString *BMKeywordKey;
extern NSString *BMLastVisitKey;
extern NSString *BMNumberVisitsKey;

// safari keys
extern NSString *SafariTypeKey;
extern NSString *SafariLeaf;
extern NSString *SafariList;
extern NSString *SafariAutoTab;
extern NSString *SafariUUIDKey;
extern NSString *SafariURIDictKey;
extern NSString *SafariBookmarkTitleKey;
extern NSString *SafariURLStringKey;

// camino XML keys
extern NSString *CaminoNameKey;
extern NSString *CaminoDescKey;
extern NSString *CaminoTypeKey;
extern NSString *CaminoKeywordKey;
extern NSString *CaminoURLKey;
extern NSString *CaminoToolbarKey;
extern NSString *CaminoDockMenuKey;
extern NSString *CaminoGroupKey;
extern NSString *CaminoBookmarkKey;
extern NSString *CaminoFolderKey;
extern NSString *CaminoTrueKey;
