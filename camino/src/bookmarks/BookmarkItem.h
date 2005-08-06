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
 *   David Haas <haasd@cae.wisc.edu>
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

// superclass for Bookmark & BookmarkFolder.
// Basically here to aid in scripting support.

#import <Appkit/Appkit.h>

enum
{
  kBookmarkItemAccumulateChangesMask    = (1 << 0),
  
  kBookmarkItemTitleChangedMask         = (1 << 1),
  kBookmarkItemDescriptionChangedMask   = (1 << 2),
  kBookmarkItemKeywordChangedMask       = (1 << 3),
  kBookmarkItemIconChangedMask          = (1 << 4),
  kBookmarkItemURLChangedMask           = (1 << 5),
  kBookmarkItemLastVisitChangedMask     = (1 << 6),
  kBookmarkItemStatusChangedMask        = (1 << 7),   // really "flags", like separator vs. bookmark
  kBookmarkItemNumVisitsChangedMask     = (1 << 8),
  
  // mask of flags that require a save of the bookmarks
  kBookmarkItemSignificantChangeFlagsMask = kBookmarkItemTitleChangedMask |
                                            kBookmarkItemDescriptionChangedMask |
                                            kBookmarkItemKeywordChangedMask |
                                            kBookmarkItemURLChangedMask |
                                            kBookmarkItemLastVisitChangedMask |
                                            kBookmarkItemStatusChangedMask |
                                            kBookmarkItemNumVisitsChangedMask,
    
  kBookmarkItemEverythingChangedMask    = 0xFFFFFFFE
};


@interface BookmarkItem : NSObject <NSCopying>
{
  id              mParent;	//subclasses will use a BookmarkFolder
  NSString*       mTitle;       
  NSString*       mDescription;
  NSString*       mKeyword; 
  NSString*       mUUID;
  NSImage*        mIcon;
  unsigned int    mPendingChangeFlags;
}

// Setters/Getters
-(id) parent;
-(NSString *) title;
-(NSString *) itemDescription;    // don't use "description"
-(NSString *) keyword;
-(NSImage *) icon;
-(NSString *) UUID;

-(void)	setParent:(id)aParent;    // note that the parent of root items is the BookmarksManager, for some reason
-(void) setTitle:(NSString *)aString;
-(void) setItemDescription:(NSString *)aString;
-(void) setKeyword:(NSString *)aKeyword;
-(void) setIcon:(NSImage *)aIcon;
-(void) setUUID:(NSString*)aUUID;

// Status checks
- (BOOL)isChildOfItem:(BookmarkItem *)anItem;
- (BOOL)hasAncestor:(BookmarkItem*)inItem;

// Searching

// search field tags, used in search field context menu item tags
enum
{
  eBookmarksSearchFieldAll = 1,
  eBookmarksSearchFieldTitle,
  eBookmarksSearchFieldURL,
  eBookmarksSearchFieldKeyword,
  eBookmarksSearchFieldDescription
};

-(BOOL)matchesString:(NSString*)searchString inFieldWithTag:(int)tag;

// Notification of Change
+(void) setSuppressAllUpdateNotifications:(BOOL)suppressUpdates;
+(BOOL) allowNotifications;
-(void) setAccumulateUpdateNotifications:(BOOL)suppressUpdates; // does not nest
-(void) itemUpdatedNote:(unsigned int)inChangeMask; // not everything triggers an item update, only certain properties changing

// Methods called on startup for both bookmark & folder
-(void) refreshIcon;

  // for reading/writing to disk - unimplemented in BookmarkItem.
-(BOOL) readNativeDictionary:(NSDictionary *)aDict;
-(BOOL) readSafariDictionary:(NSDictionary *)aDict;
-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef;

-(void)writeBookmarksMetadataToPath:(NSString*)inPath;
-(void)removeBookmarksMetadataFromPath:(NSString*)inPath;
-(NSDictionary *)writeNativeDictionary;
-(NSDictionary *)writeSafariDictionary;
-(NSString *)writeHTML:(unsigned)aPad;

// methods used for saving to files; are guaranteed never to return nil
- (id)savedTitle;
- (id)savedItemDescription;    // don't use "description"
- (id)savedKeyword;
- (id)savedUUID;    // does not generate a new UUID if UUID is not set

@end

// Bunch of Keys for reading/writing dictionaries.

// Safari & Camino plist keys
extern NSString* const BMTitleKey;
extern NSString* const BMChildrenKey;

// Camino plist keys
extern NSString* const BMFolderDescKey;
extern NSString* const BMFolderTypeKey;
extern NSString* const BMFolderKeywordKey;
extern NSString* const BMDescKey;
extern NSString* const BMStatusKey;
extern NSString* const BMURLKey;
extern NSString* const BMUUIDKey;
extern NSString* const BMKeywordKey;
extern NSString* const BMLastVisitKey;
extern NSString* const BMNumberVisitsKey;
extern NSString* const BMLinkedFaviconURLKey;

// safari keys
extern NSString* const SafariTypeKey;
extern NSString* const SafariLeaf;
extern NSString* const SafariList;
extern NSString* const SafariAutoTab;
extern NSString* const SafariUUIDKey;
extern NSString* const SafariURIDictKey;
extern NSString* const SafariBookmarkTitleKey;
extern NSString* const SafariURLStringKey;

// camino XML keys
extern NSString* const CaminoNameKey;
extern NSString* const CaminoDescKey;
extern NSString* const CaminoTypeKey;
extern NSString* const CaminoKeywordKey;
extern NSString* const CaminoURLKey;
extern NSString* const CaminoToolbarKey;
extern NSString* const CaminoDockMenuKey;
extern NSString* const CaminoGroupKey;
extern NSString* const CaminoBookmarkKey;
extern NSString* const CaminoFolderKey;
extern NSString* const CaminoTrueKey;
