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
 *   Ben Goodger <ben@netscape.com> (Original Author)
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

#import <Foundation/Foundation.h>
#import <Appkit/Appkit.h>
#import "ExtendedOutlineView.h"

class nsIRDFDataSource;
class nsIRDFContainer;
class nsIRDFContainerUtils;
class nsIRDFResource;
class nsIRDFService;


// RDF Resource Wrapper to make the Outline View happy
@interface RDFOutlineViewItem : NSObject
{
    nsIRDFResource* mResource;
}

- (nsIRDFResource*) resource;
- (void) setResource: (nsIRDFResource*) aResource;

@end


@interface RDFOutlineViewDataSource : NSObject {
    nsIRDFDataSource* 		mDataSource;
    nsIRDFContainer* 		mContainer;
    nsIRDFContainerUtils* 	mContainerUtils;
    nsIRDFResource* 		mRootResource;
    nsIRDFService* 			mRDFService;

    IBOutlet ExtendedOutlineView* mOutlineView;	

    NSMutableDictionary*	mDictionary;
}

// Initialization Methods
- (void) ensureDataSourceLoaded;
- (void) cleanupDataSource;

- (nsIRDFDataSource*) dataSource;
- (nsIRDFResource*) rootResource;
- (void) setDataSource: (nsIRDFDataSource*) aDataSource;
- (void) setRootResource: (nsIRDFResource*) aResource;

// Outline View Data Source methods
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

// tooltip support from the ExtendedOutlineView. Override to provide a tooltip
// other than the Name property for each item in the view.
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)inItem;

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren;

// Implementation Methods
- (id) makeWrapperFor: (nsIRDFResource*) aRDFResource;

// override to do something different with the cell data rather than just
// return a string (add an icon in an attributed string, for example).
-(id) createCellContents:(const nsAString&)inValue withColumn:(NSString*)inColumn byItem:(id) inItem;

-(void) getPropertyString:(NSString*)inPropertyURI forItem:(RDFOutlineViewItem*)inItem
            result:(PRUnichar**)outResult;

@end


