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

#import "NSPasteboard+Utils.h"

const NSString* kCorePasteboardFlavorType_url  = @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
const NSString* kCorePasteboardFlavorType_urln = @"CorePasteboardFlavorType 0x75726C6E"; // 'urln'  title

@implementation NSPasteboard(ChimeraPasteboardURLUtils)

- (int)declareURLPasteboardWithAdditionalTypes:(NSArray*)additionalTypes owner:(id)newOwner
{
  NSArray* allTypes = [additionalTypes arrayByAddingObjectsFromArray:
                            [NSArray arrayWithObjects:
                                        @"MozURLType",
                                        NSURLPboardType,
                                        NSStringPboardType,
                                        kCorePasteboardFlavorType_url,
                                        kCorePasteboardFlavorType_urln,
                                        nil]];
	return [self declareTypes:allTypes owner:newOwner];
}

- (void)setDataForURL:(NSString*)url title:(NSString*)title
{
  NSDictionary *data = [NSDictionary dictionaryWithObjectsAndKeys:
                            url, @"url",
                            title, @"title",
                            nil];

  [self setPropertyList:data forType: @"MozURLType"];
  [[NSURL URLWithString:url] writeToPasteboard: self];
  [self setString:url forType: NSStringPboardType];

  const char* tempCString = [url UTF8String];
  [self setData:[NSData dataWithBytes:tempCString length:strlen(tempCString)] forType: kCorePasteboardFlavorType_url];

  tempCString = [title UTF8String];
  [self setData:[NSData dataWithBytes:tempCString length:strlen(tempCString)] forType: kCorePasteboardFlavorType_urln];
}

@end

