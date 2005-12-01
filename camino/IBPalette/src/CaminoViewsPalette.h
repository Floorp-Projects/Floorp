//
//  CaminoViewsPalette.h
//  CaminoViewsPalette
//
//  Created by Simon Fraser on 21/11/05.
//  Copyright __MyCompanyName__ 2005 . All rights reserved.
//

#import <InterfaceBuilder/InterfaceBuilder.h>

#import "CHStackView.h"
#import "AutoSizingTextField.h"

@interface CaminoViewsPalette : IBPalette
{
  IBOutlet NSImageView*             mShrinkWrapViewImageView;
  IBOutlet NSImageView*             mFlippedShrinkWrapViewImageView;
  IBOutlet NSImageView*             mStackViewImageView;

  IBOutlet CHShrinkWrapView*        mShrinkWrapView;
  IBOutlet CHFlippedShrinkWrapView* mFlippedShrinkWrapView;
  IBOutlet CHStackView*             mStackView;
}

@end

@interface NSObject(CaminoViewsInspector)

+ (BOOL)editingInInterfaceBuilder;

@end

@interface CHShrinkWrapView(CaminoViewsInspector)

- (NSString *)inspectorClassName;
- (BOOL)ibIsContainer;

@end

@interface CHFlippedShrinkWrapView(CaminoViewsInspector)

- (NSString *)inspectorClassName;
- (BOOL)ibIsContainer;

@end


@interface CHStackView(CaminoViewsInspector)

- (NSString *)inspectorClassName;
- (BOOL)ibIsContainer;

@end

@interface AutoSizingTextField(CaminoViewsInspector)

- (NSString *)inspectorClassName;

@end
