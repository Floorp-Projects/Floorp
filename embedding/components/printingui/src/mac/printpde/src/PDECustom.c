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
static void InitSettings(MySettings* settings);
static void InternSettings(CFDictionaryRef srcDict, MySettings* settings);
static void ExternSettings(MySettings* settings, CFMutableDictionaryRef destDict);
static void SyncPaneFromSettings(MyCustomContext context);
static void SyncSettingsFromPane(MyCustomContext context);
static CFStringRef GetSummaryTextBooleanValue(Boolean value);
static CFStringRef GetSummaryTextNAValue();
static CFStringRef GetSummaryTextHeaderFooterValue(CFStringRef inStr);
static int  GetIndexForPrintString(CFStringRef stringCode);
static CFStringRef GetPrintStringFromIndex(int index);

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
    MyCFAssign(NULL, context->settings.mHeaderLeft);
    MyCFAssign(NULL, context->settings.mHeaderCenter);
    MyCFAssign(NULL, context->settings.mHeaderRight);
    MyCFAssign(NULL, context->settings.mFooterLeft);
    MyCFAssign(NULL, context->settings.mFooterCenter);
    MyCFAssign(NULL, context->settings.mFooterRight);
    
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
    static const ControlID headerLeftPopupControlID = { kMozPDECreatorCode, 4006 };
    static const ControlID headerCenterPopupControlID = { kMozPDECreatorCode, 4007 };
    static const ControlID headerRightPopupControlID = { kMozPDECreatorCode, 4008 };
    static const ControlID footerLeftPopupControlID = { kMozPDECreatorCode, 4009 };
    static const ControlID footerCenterPopupControlID = { kMozPDECreatorCode, 4010 };
    static const ControlID footerRightPopupControlID = { kMozPDECreatorCode, 4011 };
    
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

            GetControlByID(controlOwner,
                           &headerLeftPopupControlID,
                           &(context->controls.headerLeftPopup));

            GetControlByID(controlOwner,
                           &headerCenterPopupControlID,
                           &(context->controls.headerCenterPopup));

            GetControlByID(controlOwner,
                           &headerRightPopupControlID,
                           &(context->controls.headerRightPopup));

            GetControlByID(controlOwner,
                           &footerLeftPopupControlID,
                           &(context->controls.footerLeftPopup));

            GetControlByID(controlOwner,
                           &footerCenterPopupControlID,
                           &(context->controls.footerCenterPopup));

            GetControlByID(controlOwner,
                           &footerRightPopupControlID,
                           &(context->controls.footerRightPopup));
                           
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
    CFStringRef format = NULL;

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
 
    // Page Headers
    title = CFCopyLocalizedStringFromTableInBundle(
                        CFSTR("Page Headers"),
                        CFSTR("Localizable"),
                        MyGetBundle(),
                        "Page Headers (for summary)");
    if (title != NULL)
    {
        format = CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("%@, %@, %@"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer format (for summary)");

        if (format != NULL)
        {
            value = CFStringCreateWithFormat(NULL, NULL, format,
                            GetSummaryTextHeaderFooterValue(context->settings.mHeaderLeft),
                            GetSummaryTextHeaderFooterValue(context->settings.mHeaderCenter),
                            GetSummaryTextHeaderFooterValue(context->settings.mHeaderRight));
            if (value != NULL)
            {
                // append the title-value pair to the arrays
                CFArrayAppendValue (titleArray, title);
                CFArrayAppendValue (valueArray, value);

                CFRelease (value);
            }
            CFRelease(format);
        }
        CFRelease (title);
    }

    // Page Footers
    title = CFCopyLocalizedStringFromTableInBundle(
                        CFSTR("Page Footers"),
                        CFSTR("Localizable"),
                        MyGetBundle(),
                        "Page Footers (for summary)");
    if (title != NULL)
    {
        format = CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("%@, %@, %@"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer format (for summary)");

        if (format != NULL)
        {
            value = CFStringCreateWithFormat(NULL, NULL, format,
                            GetSummaryTextHeaderFooterValue(context->settings.mFooterLeft),
                            GetSummaryTextHeaderFooterValue(context->settings.mFooterCenter),
                            GetSummaryTextHeaderFooterValue(context->settings.mFooterRight));
            if (value != NULL)
            {
                // append the title-value pair to the arrays
                CFArrayAppendValue (titleArray, title);
                CFArrayAppendValue (valueArray, value);

                CFRelease (value);
            }
            CFRelease(format);
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
    OSStatus result = noErr;
    PMTicketRef ticket = NULL;

    result = MyGetTicket (session, kPDE_PMPrintSettingsRef, &ticket);
    if (result == noErr)
    {
        CFDataRef xmlData = NULL;
        CFDictionaryRef dict = NULL;

        result = PMTicketGetCFData (
            ticket, 
            kPMTopLevel, 
            kPMTopLevel, 
            kAppPrintDialogPDEOnlyKey, 
            &xmlData
        );
        if (result == noErr)
        {
            dict = CFPropertyListCreateFromXMLData (
                        kCFAllocatorDefault,
                        xmlData,
                        kCFPropertyListImmutable,
                        NULL
                        );
            if (dict)
            {
                InternSettings(dict, &context->settings);
                CFRelease(dict);
            }
            else
                result = kPMKeyNotFound;    // bad XML?
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
    OSStatus result = noErr;
    PMTicketRef ticket = NULL;
    CFMutableDictionaryRef dict = NULL;
    CFDataRef xmlData = NULL;

    result = MyGetTicket (session, kPDE_PMPrintSettingsRef, &ticket);
    if (result == noErr)
    {
        // If our pane has never been shown, this will be a noop.
        SyncSettingsFromPane(context);
        
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    (const CFDictionaryKeyCallBacks *)&kCFTypeDictionaryKeyCallBacks,
                    (const CFDictionaryValueCallBacks *)&kCFTypeDictionaryValueCallBacks);
        if (dict)
        {
            ExternSettings(&context->settings, dict);
            xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);
            if (xmlData)
            {
                result = PMTicketSetCFData (
                    ticket, 
                    kMyBundleIdentifier, 
                    kAppPrintDialogPDEOnlyKey, 
                    xmlData, 
                    kPMUnlocked
                );
                CFRelease(xmlData);
            }
            CFRelease(dict);
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

static void InitSettings(MySettings* settings)
{
    settings->mHaveSelection                = false;
    settings->mHaveFrames                   = false;
    settings->mHaveFrameSelected            = false;
  
    settings->mPrintFrameAsIs               = false;
    settings->mPrintSelectedFrame           = false;
    settings->mPrintFramesSeparately        = false;
    settings->mPrintSelection               = false;
    settings->mShrinkToFit                  = true;
    settings->mPrintBGColors                = true;
    settings->mPrintBGImages                = true;
    
    settings->mHeaderLeft                   = MyCFAssign(NULL, settings->mHeaderLeft);
    settings->mHeaderCenter                 = MyCFAssign(NULL, settings->mHeaderCenter);
    settings->mHeaderRight                  = MyCFAssign(NULL, settings->mHeaderRight);
    settings->mFooterLeft                   = MyCFAssign(NULL, settings->mFooterLeft);
    settings->mFooterCenter                 = MyCFAssign(NULL, settings->mFooterCenter);
    settings->mFooterRight                  = MyCFAssign(NULL, settings->mFooterRight);
}

static void InternSettings(CFDictionaryRef srcDict, MySettings* settings)
{
    CFTypeRef dictValue;
    
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyHaveSelection)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mHaveSelection = CFBooleanGetValue((CFBooleanRef)dictValue);

    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyHaveFrames)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mHaveFrames = CFBooleanGetValue((CFBooleanRef)dictValue);

    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyHaveFrameSelected)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mHaveFrameSelected = CFBooleanGetValue((CFBooleanRef)dictValue);
    
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyPrintFrameType)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID())) {
        if (CFEqual(dictValue, kPDEValueFramesAsIs))
            settings->mPrintFrameAsIs = true;
        else if (CFEqual(dictValue, kPDEValueSelectedFrame))
            settings->mPrintSelectedFrame = true;
        else if (CFEqual(dictValue, kPDEValueEachFrameSep))
            settings->mPrintFramesSeparately = true;
    }

    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyPrintSelection)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mPrintSelection = CFBooleanGetValue((CFBooleanRef)dictValue); 
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyShrinkToFit)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mShrinkToFit = CFBooleanGetValue((CFBooleanRef)dictValue);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyPrintBGColors)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mPrintBGColors = CFBooleanGetValue((CFBooleanRef)dictValue);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyPrintBGImages)) &&
        (CFGetTypeID(dictValue) == CFBooleanGetTypeID()))
            settings->mPrintBGImages = CFBooleanGetValue((CFBooleanRef)dictValue);
    
    
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyHeaderLeft)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID()))
            settings->mHeaderLeft = MyCFAssign(dictValue, settings->mHeaderLeft);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyHeaderCenter)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID()))
            settings->mHeaderCenter = MyCFAssign(dictValue, settings->mHeaderCenter);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyHeaderRight)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID()))
            settings->mHeaderRight = MyCFAssign(dictValue, settings->mHeaderRight);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyFooterLeft)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID()))
            settings->mFooterLeft = MyCFAssign(dictValue, settings->mFooterLeft);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyFooterCenter)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID()))
            settings->mFooterCenter = MyCFAssign(dictValue, settings->mFooterCenter);
    if ((dictValue = CFDictionaryGetValue(srcDict, kPDEKeyFooterRight)) &&
        (CFGetTypeID(dictValue) == CFStringGetTypeID()))
            settings->mFooterRight = MyCFAssign(dictValue, settings->mFooterRight);
}

static void ExternSettings(MySettings* settings, CFMutableDictionaryRef destDict)
{
    // Do not write the state info mHaveSelection, mHaveFrames, mHaveFrameSelected
    // That info is used to enable/disable UI for this session only. It's not read
    // back by the application, nor should it be saved in custom print settings
    
    if (settings->mPrintFrameAsIs)
        CFDictionaryAddValue(destDict, kPDEKeyPrintFrameType, kPDEValueFramesAsIs);
    else if (settings->mPrintSelectedFrame)
        CFDictionaryAddValue(destDict, kPDEKeyPrintFrameType, kPDEValueSelectedFrame);
    else if (settings->mPrintFramesSeparately)
        CFDictionaryAddValue(destDict, kPDEKeyPrintFrameType, kPDEValueEachFrameSep);

    CFDictionaryAddValue(destDict, kPDEKeyPrintSelection, settings->mPrintSelection ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionaryAddValue(destDict, kPDEKeyShrinkToFit, settings->mShrinkToFit ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionaryAddValue(destDict, kPDEKeyPrintBGColors, settings->mPrintBGColors ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionaryAddValue(destDict, kPDEKeyPrintBGImages, settings->mPrintBGImages ? kCFBooleanTrue : kCFBooleanFalse);
    
    CFDictionaryAddValue(destDict, kPDEKeyHeaderLeft, settings->mHeaderLeft ? settings->mHeaderLeft : CFSTR(""));
    CFDictionaryAddValue(destDict, kPDEKeyHeaderCenter, settings->mHeaderCenter ? settings->mHeaderCenter : CFSTR(""));
    CFDictionaryAddValue(destDict, kPDEKeyHeaderRight, settings->mHeaderRight ? settings->mHeaderRight : CFSTR(""));
    CFDictionaryAddValue(destDict, kPDEKeyFooterLeft, settings->mFooterLeft ? settings->mFooterLeft : CFSTR(""));
    CFDictionaryAddValue(destDict, kPDEKeyFooterCenter, settings->mFooterCenter ? settings->mFooterCenter : CFSTR(""));
    CFDictionaryAddValue(destDict, kPDEKeyFooterRight, settings->mFooterRight ? settings->mFooterRight : CFSTR(""));
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
                SetControl32BitValue(context->controls.frameRadioGroup,
                                kFramesSelectedIndex);
            else if (context->settings.mPrintFramesSeparately)
                SetControl32BitValue(context->controls.frameRadioGroup,
                                kFramesEachSeparatelyIndex);
            else /* (context->settings.mPrintFrameAsIs) */
                SetControl32BitValue(context->controls.frameRadioGroup,
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
            SetControl32BitValue(context->controls.frameRadioGroup, 0);
        }
    }
    
    if (context->controls.printSelCheck)
    {
        if (context->settings.mHaveSelection)
        {
            EnableControl(context->controls.printSelCheck);
            SetControl32BitValue(context->controls.printSelCheck,
                            context->settings.mPrintSelection);
        }
        else
        {
            DisableControl(context->controls.printSelCheck);
            SetControl32BitValue(context->controls.printSelCheck, 0);
        }
    }
    if (context->controls.shrinkToFitCheck)
        SetControl32BitValue(context->controls.shrinkToFitCheck,
                        context->settings.mShrinkToFit);
    if (context->controls.printBGColorsCheck)
        SetControl32BitValue(context->controls.printBGColorsCheck,
                        context->settings.mPrintBGColors);
    if (context->controls.printBGImagesCheck)
        SetControl32BitValue(context->controls.printBGImagesCheck,
                        context->settings.mPrintBGImages);
                        
    if (context->controls.headerLeftPopup)
      SetControl32BitValue(context->controls.headerLeftPopup,
                        GetIndexForPrintString(context->settings.mHeaderLeft));
    if (context->controls.headerCenterPopup)
      SetControl32BitValue(context->controls.headerCenterPopup,
                        GetIndexForPrintString(context->settings.mHeaderCenter));
    if (context->controls.headerRightPopup)
      SetControl32BitValue(context->controls.headerRightPopup,
                        GetIndexForPrintString(context->settings.mHeaderRight));

    if (context->controls.footerLeftPopup)
      SetControl32BitValue(context->controls.footerLeftPopup,
                        GetIndexForPrintString(context->settings.mFooterLeft));
    if (context->controls.footerCenterPopup)
      SetControl32BitValue(context->controls.footerCenterPopup,
                        GetIndexForPrintString(context->settings.mFooterCenter));
    if (context->controls.footerRightPopup)
      SetControl32BitValue(context->controls.footerRightPopup,
                         GetIndexForPrintString(context->settings.mFooterRight));

}


static void SyncSettingsFromPane(MyCustomContext context)
{
    SInt32 controlVal;
    
    if (context->controls.frameRadioGroup)
    {
        context->settings.mPrintFrameAsIs = false;
        context->settings.mPrintSelectedFrame = false;
        context->settings.mPrintFramesSeparately = false;
        switch (GetControl32BitValue(context->controls.frameRadioGroup))
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
          GetControl32BitValue(context->controls.printSelCheck);
    if (context->controls.printSelCheck)
        context->settings.mShrinkToFit = 
          GetControl32BitValue(context->controls.shrinkToFitCheck);
    if (context->controls.printSelCheck)
        context->settings.mPrintBGColors = 
          GetControl32BitValue(context->controls.printBGColorsCheck);
    if (context->controls.printSelCheck)
        context->settings.mPrintBGImages = 
          GetControl32BitValue(context->controls.printBGImagesCheck);
          
    if (context->controls.headerLeftPopup) {
      controlVal = GetControl32BitValue(context->controls.headerLeftPopup);
      context->settings.mHeaderLeft = MyCFAssign(GetPrintStringFromIndex(controlVal),
                                                 context->settings.mHeaderLeft);      
    }
    if (context->controls.headerCenterPopup) {
      controlVal = GetControl32BitValue(context->controls.headerCenterPopup);
      context->settings.mHeaderCenter = MyCFAssign(GetPrintStringFromIndex(controlVal),
                                                   context->settings.mHeaderCenter);
    }
    if (context->controls.headerRightPopup) {
      controlVal = GetControl32BitValue(context->controls.headerRightPopup);
      context->settings.mHeaderRight = MyCFAssign(GetPrintStringFromIndex(controlVal),
                                                  context->settings.mHeaderRight);
    }
    if (context->controls.footerLeftPopup) {
      controlVal = GetControl32BitValue(context->controls.footerLeftPopup);
      context->settings.mFooterLeft = MyCFAssign(GetPrintStringFromIndex(controlVal),
                                      context->settings.mFooterLeft);
    }
    if (context->controls.footerCenterPopup) {
      controlVal = GetControl32BitValue(context->controls.footerCenterPopup);
      context->settings.mFooterCenter = MyCFAssign(GetPrintStringFromIndex(controlVal),
                                                   context->settings.mFooterCenter);
    }
    if (context->controls.footerRightPopup) {
      controlVal = GetControl32BitValue(context->controls.footerRightPopup);
      context->settings.mFooterRight = MyCFAssign(GetPrintStringFromIndex(controlVal),
                                                  context->settings.mFooterRight);
    }

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

static CFStringRef GetSummaryTextHeaderFooterValue(CFStringRef inStr)
{   
    if (!inStr || !CFStringGetLength(inStr))
        return CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("(Blank)"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer <blank> (for summary)");
    else if (CFEqual(inStr, CFSTR("&T")))
        return CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("Title"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer <title> (for summary)");
    else if (CFEqual(inStr, CFSTR("&U")))
        return CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("URL"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer <url> (for summary)");
    else if (CFEqual(inStr, CFSTR("&D")))
        return CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("Date"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer <date> (for summary)");
    else if (CFEqual(inStr, CFSTR("&P")))
        return CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("Page #"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer <page #> (for summary)");
    else if (CFEqual(inStr, CFSTR("&PT")))
        return CFCopyLocalizedStringFromTableInBundle(
                            CFSTR("Page # of #"),
                            CFSTR("Localizable"),
                            MyGetBundle(),
                            "Page Heaader/Footer <page # of #> (for summary)");
    else
        return CFRetain(inStr);
}

//
// Here we have a mapping of control codes to pop-up button messages
//

static int  GetIndexForPrintString(CFStringRef stringCode)
{
    if (stringCode)
    {
        // title
        if (CFEqual(stringCode, CFSTR("&T")))
            return 2;
        // URL
        if (CFEqual(stringCode, CFSTR("&U")))
            return 3;
        // Date/Time
        if (CFEqual(stringCode, CFSTR("&D")))
            return 4;
        // Page Number
        if (CFEqual(stringCode, CFSTR("&P")))
            return 5;
        // Page Number of Total Page Numbers
        if (CFEqual(stringCode, CFSTR("&PT")))
            return 6;
    }
    // Something Else - aka "Blank"
    return 1;
}

CFStringRef GetPrintStringFromIndex(int index)
{
    switch (index)
    {
        case 2: // title
            return CFSTR("&T");
        case 3: // url
            return CFSTR("&U");
        case 4: // date/time
            return CFSTR("&D");
        case 5: // Page #
            return CFSTR("&P");
        case 6: // Page # of #
            return CFSTR("&PT");
        default: // blank OR custom (need to implement custom)
            return CFSTR("");
    }
 }

/* END OF SOURCE */
