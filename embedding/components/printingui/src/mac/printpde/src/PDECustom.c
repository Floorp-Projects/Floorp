/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Conrad Carlen <ccarlen@netscape.com>
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

#include <Carbon/Carbon.h>
#include <Print/PMPrintingDialogExtensions.h>

#include "PDECore.h"
#include "PDECustom.h"
#include "PDEUtilities.h"

// Static Prototypes
static void InitSettings(nsPrintExtensions* settings);
static void SyncPaneFromSettings(MyCustomContext context);
static void SyncSettingsFromPane(MyCustomContext context);
static CFStringRef GetSummaryTextBooleanValue(Boolean value);
static CFStringRef GetSummaryTextNAValue();

/*
--------------------------------------------------------------------------------
    MyCreateCustomContext
--------------------------------------------------------------------------------
*/

extern MyCustomContext MyCreateCustomContext()
{
    // allocate zeroed storage for a custom context
    MyCustomContext context = calloc (1, sizeof (MyCustomContextBlock));

    return context;
}


/*
--------------------------------------------------------------------------------
    MyReleaseCustomContext
--------------------------------------------------------------------------------
*/

extern void MyReleaseCustomContext (MyCustomContext context)
{
    free (context);
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MyGetCustomTitle
--------------------------------------------------------------------------------
*/

/*
    In this implementation, we only copy the localized title string once.
    We keep a static reference to the copy for re-use.
*/

extern CFStringRef MyGetCustomTitle (Boolean stillNeeded)
{
    static CFStringRef sTitle = NULL;

    if (stillNeeded)
    {
        if (sTitle == NULL)
        {
            // Get the name of the application hosting us.
            CFBundleRef appBundle = CFBundleGetMainBundle();
            if (appBundle)
            {
                CFStringRef bundleString = CFBundleGetValueForInfoDictionaryKey(
                                            appBundle, CFSTR("CFBundleName"));
                // We don't get an owning reference here, so make a copy.
                if (bundleString && (CFGetTypeID(bundleString) == CFStringGetTypeID()))
                    sTitle = CFStringCreateCopy(NULL, bundleString);
            }
        }
        // If that failed, use a resource - we may be hosted by an unbundled app.
        if (sTitle == NULL)
        {
            sTitle = CFCopyLocalizedStringFromTableInBundle (
                CFSTR("Web Browser"),
                CFSTR("Localizable"),
                MyGetBundle(),
                "the custom pane title");
        }
    }
    else 
    {
        if (sTitle != NULL)
        {
            CFRelease (sTitle);
            sTitle = NULL;
        }
    }

    return sTitle;
}


/*
--------------------------------------------------------------------------------
    MyEmbedCustomControls
--------------------------------------------------------------------------------
*/

extern OSStatus MyEmbedCustomControls (
    MyCustomContext context,
    WindowRef nibWindow,
    ControlRef userPane
)

{
    static const ControlID containerControlID = { kMozPDECreatorCode, 4000 };
    static const ControlID radioGroupControlID = { kMozPDECreatorCode, 4001 };
    static const ControlID printSelCheckControlID = { kMozPDECreatorCode, 4002 };
    static const ControlID shrinkToFitCheckControlID = { kMozPDECreatorCode, 4003 };
    static const ControlID printBGColorsCheckControlID = { kMozPDECreatorCode, 4004 };
    static const ControlID printBGImagesCheckControlID = { kMozPDECreatorCode, 4005 };
    
    OSStatus result = noErr;
    
    if (context != NULL)
    {
        ControlHandle paneControl = NULL;
        
        // The control we're embedding into the given
        // userPane is itself a user pane control.
        result = MyEmbedControl(nibWindow,
                                userPane,
                                &containerControlID,
                                &paneControl);
        
        if (paneControl)
        {
            WindowRef controlOwner = GetControlOwner(paneControl);
            
            GetControlByID(controlOwner,
                           &radioGroupControlID,
                           &(context->controls.frameRadioGroup));
            if (context->controls.frameRadioGroup != NULL)
            {

                // It doesn't seem to be possible to specify the titles of the
                // radio buttons within a radio group control, so do it by hand :-/
                // This is not done as a loop, but instead using CFSTR("abc") so
                // that genstrings can grok this file. Maybe that's not worth it?
                
                CFStringRef radioTitle;
                ControlRef radioControl;
                    
                if (GetIndexedSubControl(context->controls.frameRadioGroup,
                                         kFramesAsLaidOutIndex, &radioControl) == noErr)
                {
                    radioTitle = CFCopyLocalizedStringFromTableInBundle(
                                        CFSTR("As laid out on the screen"),
                                        CFSTR("Localizable"),
                                        MyGetBundle(),
                                        "top radio title");
                    if (radioTitle)
                    {
                        SetControlTitleWithCFString(radioControl, radioTitle);
                        CFRelease(radioTitle);
                    }
                }
                if (GetIndexedSubControl(context->controls.frameRadioGroup,
                                         kFramesSelectedIndex, &radioControl) == noErr)
                {
                    radioTitle = CFCopyLocalizedStringFromTableInBundle(
                                        CFSTR("The selected frame"),
                                        CFSTR("Localizable"),
                                        MyGetBundle(),
                                        "middle radio title");
                    if (radioTitle)
                    {
                        SetControlTitleWithCFString(radioControl, radioTitle);
                        CFRelease(radioTitle);
                    }
                }
                if (GetIndexedSubControl(context->controls.frameRadioGroup,
                                         kFramesEachSeparatelyIndex, &radioControl) == noErr)
                {
                    radioTitle = CFCopyLocalizedStringFromTableInBundle(
                                        CFSTR("Each frame separately"),
                                        CFSTR("Localizable"),
                                        MyGetBundle(),
                                        "bottom radio title");
                    if (radioTitle)
                    {
                        SetControlTitleWithCFString(radioControl, radioTitle);
                        CFRelease(radioTitle);
                    }
                }                
            }
 
            GetControlByID(controlOwner,
                           &printSelCheckControlID,
                           &(context->controls.printSelCheck));
            GetControlByID(controlOwner,
                           &shrinkToFitCheckControlID,
                           &(context->controls.shrinkToFitCheck));
            GetControlByID(controlOwner,
                           &printBGColorsCheckControlID,
                           &(context->controls.printBGColorsCheck));
            GetControlByID(controlOwner,
                           &printBGImagesCheckControlID,
                           &(context->controls.printBGImagesCheck));

            // Now that the controls are in, sync with data.
            SyncPaneFromSettings(context);
        }
    }

    return result;
}


/*
--------------------------------------------------------------------------------
    MyGetSummaryText
--------------------------------------------------------------------------------
*/

extern OSStatus MyGetSummaryText (
    MyCustomContext context, 
    CFMutableArrayRef titleArray,
    CFMutableArrayRef valueArray
)

{
    CFStringRef title = NULL;
    CFStringRef value = NULL;

    OSStatus result = noErr;

    // "Print Frames" Radio Group
    title = CFCopyLocalizedStringFromTableInBundle (
                CFSTR("Print Frames"),
                CFSTR("Localizable"),
                MyGetBundle(),
                "the Print Frames radio group (for summary)");

    if (title != NULL)
    {        
        if (context->settings.mHaveFrames)
        {
            if (context->settings.mPrintFrameAsIs)
                value = CFCopyLocalizedStringFromTableInBundle(
                                    CFSTR("As laid out on the screen"),
                                    CFSTR("Localizable"),
                                    MyGetBundle(),
                                    "Print Frames choice #1 (for summary)");
            else if (context->settings.mPrintSelectedFrame)
                value = CFCopyLocalizedStringFromTableInBundle(
                                    CFSTR("The selected frame"),
                                    CFSTR("Localizable"),
                                    MyGetBundle(),
                                    "Print Frames choice #2 (for summary)");
            else if (context->settings.mPrintFramesSeparately)
                value = CFCopyLocalizedStringFromTableInBundle(
                                    CFSTR("Each frame separately"),
                                    CFSTR("Localizable"),
                                    MyGetBundle(),
                                    "Print Frames choice #3 (for summary)");
        }
        else
            value = GetSummaryTextNAValue();
        
        if (value != NULL)
        {
            // append the title-value pair to the arrays
            CFArrayAppendValue (titleArray, title);
            CFArrayAppendValue (valueArray, value);
            
            CFRelease (value);
        }
        CFRelease (title);
    }

    // Print Selection
    title = CFCopyLocalizedStringFromTableInBundle(
                        CFSTR("Print Selection"),
                        CFSTR("Localizable"),
                        MyGetBundle(),
                        "Print Selection title (for summary)");
    if (title != NULL)
    {
        if (context->settings.mHaveSelection)
            value = GetSummaryTextBooleanValue(context->settings.mPrintSelection);
        else
            value = GetSummaryTextNAValue();

        if (value != NULL)
        {
            // append the title-value pair to the arrays
            CFArrayAppendValue (titleArray, title);
            CFArrayAppendValue (valueArray, value);

            CFRelease (value);
        }
        CFRelease (title);
    }

    // Shrink To Fit
    title = CFCopyLocalizedStringFromTableInBundle(
                        CFSTR("Shrink To Fit"),
                        CFSTR("Localizable"),
                        MyGetBundle(),
                        "Shrink To Fit title (for summary)");
    if (title != NULL)
    {
        value = GetSummaryTextBooleanValue(context->settings.mShrinkToFit);
        if (value != NULL)
        {
            // append the title-value pair to the arrays
            CFArrayAppendValue (titleArray, title);
            CFArrayAppendValue (valueArray, value);

            CFRelease (value);
        }
        CFRelease (title);
    }

    // Print BG Colors
    title = CFCopyLocalizedStringFromTableInBundle(
                        CFSTR("Print BG Colors"),
                        CFSTR("Localizable"),
                        MyGetBundle(),
                        "Print BG Colors title (for summary)");
    if (title != NULL)
    {
        value = GetSummaryTextBooleanValue(context->settings.mPrintBGColors);
        if (value != NULL)
        {
            // append the title-value pair to the arrays
            CFArrayAppendValue (titleArray, title);
            CFArrayAppendValue (valueArray, value);

            CFRelease (value);
        }
        CFRelease (title);
    }

    // Print BG Images
    title = CFCopyLocalizedStringFromTableInBundle(
                        CFSTR("Print BG Images"),
                        CFSTR("Localizable"),
                        MyGetBundle(),
                        "Print BG Images title (for summary)");
    if (title != NULL)
    {
        value = GetSummaryTextBooleanValue(context->settings.mPrintBGImages);
        if (value != NULL)
        {
            // append the title-value pair to the arrays
            CFArrayAppendValue (titleArray, title);
            CFArrayAppendValue (valueArray, value);

            CFRelease (value);
        }
        CFRelease (title);
    }
    
    return result;
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MySyncPaneFromTicket
--------------------------------------------------------------------------------
*/

extern OSStatus MySyncPaneFromTicket (
    MyCustomContext context, 
    PMPrintSession session
)

{
    CFDataRef data = NULL;
    CFIndex length = 0;
    OSStatus result = noErr;
    PMTicketRef ticket = NULL;

    result = MyGetTicket (session, kPDE_PMPrintSettingsRef, &ticket);

    if (result == noErr)
    {
        // copy ticket data into our settings
        result = PMTicketGetCFData (
            ticket, 
            kPMTopLevel, 
            kPMTopLevel, 
            kMyAppPrintSettingsKey, 
            &data
        );
    
        if (result == noErr)
        {
            length = CFDataGetLength (data);

            if (length == sizeof(nsPrintExtensions))
            {
                CFDataGetBytes (
                    data, 
                    CFRangeMake(0,length), 
                    (UInt8*) &(context->settings)
                );
            }
            else
            {   // so below we use the default value for mismatched length
                result = kPMKeyNotFound;
            }
        }
        
        if (result == kPMKeyNotFound) 
        {
            InitSettings(&context->settings);
            result = noErr;
        }
    }

    if (result == noErr)
        SyncPaneFromSettings(context);
    
    MyDebugMessage("MySyncPane", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MySyncTicketFromPane
--------------------------------------------------------------------------------
*/

extern OSStatus MySyncTicketFromPane (
    MyCustomContext context, 
    PMPrintSession session
)

{
    CFDataRef data = NULL;
    OSStatus result = noErr;
    PMTicketRef ticket = NULL;

    result = MyGetTicket (session, kPDE_PMPrintSettingsRef, &ticket);
    if (result == noErr)
    {
        // If our pane has never been shown, this will be a noop.
        SyncSettingsFromPane(context);
        
        data = CFDataCreate (
            kCFAllocatorDefault, 
            (UInt8*) &context->settings, 
            sizeof(nsPrintExtensions)
        );
    
        if (data != NULL)
        {
            // update ticket
            result = PMTicketSetCFData (
                ticket, 
                kMyBundleIdentifier, 
                kMyAppPrintSettingsKey, 
                data, 
                kPMUnlocked
            );

            CFRelease (data);
        }
    }

    MyDebugMessage("MySyncTicket", result);
    return result;
}

#pragma mark -

/*
--------------------------------------------------------------------------------
    Static Routines
--------------------------------------------------------------------------------
*/

static void InitSettings(nsPrintExtensions* settings)
{
    settings->mHaveSelection                = false;
    settings->mHaveFrames                   = false;
    settings->mHaveFrameSelected            = false;
  
    settings->mPrintFrameAsIs               = false;
    settings->mPrintSelectedFrame           = false;
    settings->mPrintFramesSeparately         = false;
    settings->mPrintSelection               = false;
    settings->mShrinkToFit                  = true;
    settings->mPrintBGColors                = true;
    settings->mPrintBGImages                = true;
}


static void SyncPaneFromSettings(MyCustomContext context)
{
    if (context->controls.frameRadioGroup)
    {
        if (context->settings.mHaveFrames)
        {
            EnableControl(context->controls.frameRadioGroup);

            if (context->settings.mPrintSelectedFrame &&
                    context->settings.mHaveFrameSelected)
                SetControlValue(context->controls.frameRadioGroup,
                                kFramesSelectedIndex);
            else if (context->settings.mPrintFramesSeparately)
                SetControlValue(context->controls.frameRadioGroup,
                                kFramesEachSeparatelyIndex);
            else /* (context->settings.mPrintFrameAsIs) */
                SetControlValue(context->controls.frameRadioGroup,
                                kFramesAsLaidOutIndex);
            
            if (!context->settings.mHaveFrameSelected)
            {
                ControlRef radioControl;
                if (GetIndexedSubControl(context->controls.frameRadioGroup,
                                          kFramesSelectedIndex, &radioControl) == noErr)
                    DisableControl(radioControl);
            }
        }
        else
        {
            DisableControl(context->controls.frameRadioGroup);
            SetControlValue(context->controls.frameRadioGroup, 0);
        }
    }
    
    if (context->controls.printSelCheck)
    {
        if (context->settings.mHaveSelection)
        {
            EnableControl(context->controls.printSelCheck);
            SetControlValue(context->controls.printSelCheck,
                            context->settings.mPrintSelection);
        }
        else
        {
            DisableControl(context->controls.printSelCheck);
            SetControlValue(context->controls.printSelCheck, 0);
        }
    }
    if (context->controls.shrinkToFitCheck)
        SetControlValue(context->controls.shrinkToFitCheck,
                        context->settings.mShrinkToFit);
    if (context->controls.printBGColorsCheck)
        SetControlValue(context->controls.printBGColorsCheck,
                        context->settings.mPrintBGColors);
    if (context->controls.printBGImagesCheck)
        SetControlValue(context->controls.printBGImagesCheck,
                        context->settings.mPrintBGImages);
}


static void SyncSettingsFromPane(MyCustomContext context)
{
    if (context->controls.frameRadioGroup)
    {
        context->settings.mPrintFrameAsIs = false;
        context->settings.mPrintSelectedFrame = false;
        context->settings.mPrintFramesSeparately = false;
        switch (GetControlValue(context->controls.frameRadioGroup))
        {
            case kFramesAsLaidOutIndex:
              context->settings.mPrintFrameAsIs = true;
              break;
            case kFramesSelectedIndex:
              context->settings.mPrintSelectedFrame = true;
              break;
            case kFramesEachSeparatelyIndex:
              context->settings.mPrintFramesSeparately = true;
              break;
        }
    }
    
    if (context->controls.printSelCheck)
        context->settings.mPrintSelection =
          GetControlValue(context->controls.printSelCheck);
    if (context->controls.printSelCheck)
        context->settings.mShrinkToFit = 
          GetControlValue(context->controls.shrinkToFitCheck);
    if (context->controls.printSelCheck)
        context->settings.mPrintBGColors = 
          GetControlValue(context->controls.printBGColorsCheck);
    if (context->controls.printSelCheck)
        context->settings.mPrintBGImages = 
          GetControlValue(context->controls.printBGImagesCheck);
}

#pragma mark -

CFStringRef GetSummaryTextBooleanValue(Boolean value)
{
    if (value)
        return CFCopyLocalizedStringFromTableInBundle(
                    CFSTR("On"),
                    CFSTR("Localizable"),
                    MyGetBundle(),
                    CFSTR("the value of a checkbox when selected"));        

    return CFCopyLocalizedStringFromTableInBundle(
                CFSTR("Off"),
                CFSTR("Localizable"),
                MyGetBundle(),
                CFSTR("the value of a checkbox when not selected"));
}  


static CFStringRef GetSummaryTextNAValue()
{
    return CFCopyLocalizedStringFromTableInBundle(
                CFSTR("N/A"),
                CFSTR("Localizable"),
                MyGetBundle(),
                "Not Applicable (for summary)");
    
}

/* END OF SOURCE */
